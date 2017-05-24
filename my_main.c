/*========== my_main.c ==========

  This is the only file you need to modify in order
  to get a working mdl project (for now).

  my_main.c will serve as the interpreter for mdl.
  When an mdl script goes through a lexer and parser,
  the resulting operations will be in the array op[].

  Your job is to go through each entry in op and perform
  the required action from the list below:

  push: push a new origin matrix onto the origin stack
  pop: remove the top matrix on the origin stack

  move/scale/rotate: create a transformation matrix
                     based on the provided values, then
		     multiply the current top of the
		     origins stack by it.

  box/sphere/torus: create a solid object based on the
                    provided values. Store that in a
		    temporary matrix, multiply it by the
		    current top of the origins stack, then
		    call draw_polygons.

  line: create a line based on the provided values. Store
        that in a temporary matrix, multiply it by the
	current top of the origins stack, then call draw_lines.

  save: call save_extension with the provided filename

  display: view the image live

  jdyrlandweaver
  =========================*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include "parser.h"
#include "symtab.h"
#include "y.tab.h"

#include "matrix.h"
#include "ml6.h"
#include "display.h"
#include "draw.h"
#include "stack.h"

/*======== void first_pass() ==========
  Inputs:
  Returns:

  Checks the op array for any animation commands
  (frames, basename, vary)

  Should set num_frames and basename if the frames
  or basename commands are present

  If vary is found, but frames is not, the entire
  program should exit.

  If frames is found, but basename is not, set name
  to some default value, and print out a message
  with the name being used.

  jdyrlandweaver
  ====================*/
void first_pass() {
  //in order to use name and num_frames
  //they must be extern variables
  extern int num_frames;
  extern char name[128];
  char vary_found = 0;
  char frames_found = 0;
  char base_found = 0;

  for (int i=0; i<lastop; i++){
    printf("%d:", i);
      switch(op[i].opcode){
        case BASENAME:
          base_found = 1;
          strcpy(name, op[i].op.basename.p->name);
          break;

        case VARY:
          vary_found = 1;
          break;

        case FRAMES:
          frames_found = 1;
          num_frames = op[i].op.frames.num_frames;
          break;
      }
      if (vary_found && (!frames_found)){
        printf("vary found but frames not found\n");
        exit(0);
      }
      if (frames_found && (!base_found)){
        printf("basename not found, set to default value (img)\n");
        strcpy(name, "img");
      }

      if (!frames_found){
        printf("frames not specified, defaulted to 1");
        num_frames = 1;
      }
  }


  return;
}

/*======== struct vary_node ** second_pass() ==========
  Inputs:    DontAskValeriy
  Returns: An array of vary_node linked lists

  In order to set the knobs for animation, we need to keep
  a seperate value for each knob for each frame. We can do
  this by using an array of linked lists. Each array index
  will correspond to a frame (eg. knobs[0] would be the first
  frame, knobs[2] would be the 3rd frame and so on).

  Each index should contain a linked list of vary_nodes, each
  node contains a knob name, a value, and a pointer to the
  next node.

  Go through the opcode array, and when you find vary, go
  from knobs[0] to knobs[frames-1] and add (or modify) the
  vary_node corresponding to the given knob with the
  appropirate value.

  jdyrlandweaver
  ====================*/
  struct vary_node ** second_pass(){
    struct vary_node ** knobs;
    struct vary_node * knob;
    knobs = (struct vary_node **)malloc(sizeof(struct vary_node*)*num_frames);
    int i,j,start_frame,end_frame;
    double start_val,end_val,cur_val,incr;
    for(i=0;i<lastop;i++){
      if(op[i].opcode==VARY){
        if (op[i].op.vary.start_frame < 0
          || op[i].op.vary.end_frame >= num_frames
        ||op[i].op.vary.start_frame > op[i].op.vary.end_frame){
          printf("invalid vary range\n");
          exit(0);
     }
        start_frame = op[i].op.vary.start_frame;
        end_frame = op[i].op.vary.end_frame;
        start_val = op[i].op.vary.start_val;
        end_val = op[i].op.vary.end_val;
        cur_val=start_val;
        incr = (end_val-start_val)/(end_frame-start_frame);
        for(j=start_frame;j<=end_frame;j++){
          knob = (struct vary_node*)malloc(sizeof(struct vary_node));
          strcpy(knob->name,op[i].op.vary.p->name);
          knob->value = cur_val;
          knob->next=knobs[j];
          knobs[j]=knob;
          cur_val+=incr;
        }
      }
    }
    return knobs;
  }


/*======== void print_knobs() ==========
Inputs:
Returns:

Goes through symtab and display all the knobs and their
currnt values

jdyrlandweaver
====================*/
void print_knobs() {

  int i;

  printf( "ID\tNAME\t\tTYPE\t\tVALUE\n" );
  for ( i=0; i < lastsym; i++ ) {

    if ( symtab[i].type == SYM_VALUE ) {
      printf( "%d\t%s\t\t", i, symtab[i].name );

      printf( "SYM_VALUE\t");
      printf( "%6.2f\n", symtab[i].s.value);
    }
  }
}

void process_knobs(int frame, struct vary_node ** knobs){
  struct vary_node * curr_node = knobs[frame];
  while (curr_node != NULL){
    set_value(lookup_symbol(curr_node->name), curr_node->value);
    curr_node = curr_node->next;
  }
}

/*======== void my_main() ==========
  Inputs:
  Returns:

  This is the main engine of the interpreter, it should
  handle most of the commadns in mdl.

  If frames is not present in the source (and therefore
  num_frames is 1, then process_knobs should be called.

  If frames is present, the entire op array must be
  applied frames time. At the end of each frame iteration
  save the current screen to a file named the
  provided basename plus a numeric string such that the
  files will be listed in order, then clear the screen and
  reset any other data structures that need it.

  Important note: you cannot just name your files in
  regular sequence, like pic0, pic1, pic2, pic3... if that
  is done, then pic1, pic10, pic11... will come before pic2
  and so on. In order to keep things clear, add leading 0s
  to the numeric portion of the name. If you use sprintf,
  you can use "%0xd" for this purpose. It will add at most
  x 0s in front of a number, if needed, so if used correctly,
  and x = 4, you would get numbers like 0001, 0002, 0011,
  0487

  jdyrlandweaver
  ====================*/
  void my_main() {

    int j,k;
    struct matrix *tmp;
    struct stack *systems;
    screen t;
    color g;
    double step = 0.1;
    double theta;
    num_frames = 1;
    struct vary_node ** knobs;
    struct vary_node * knob;
    systems = new_stack();
    tmp = new_matrix(4, 1000);
    clear_screen(t);
    g.red = 255;
    g.green = 255;
    g.blue = 255;
    double xval,yval,zval,kval;
    first_pass();
    knobs = (struct vary_node **)malloc(sizeof(struct vary_node*)*num_frames);
    if(num_frames>1)knobs=second_pass();
    for(j=0;j<num_frames;j++){
      if(num_frames>1){
        for(knob=knobs[j];knob!=NULL;knob=knob->next)
          set_value(lookup_symbol(knob->name),knob->value);
        }
    for (int i=0;i<lastop;i++) {
        switch (op[i].opcode)
  	{
  	case SPHERE:
  	  if (op[i].op.sphere.constants != NULL)
  	    {
  	      printf("\tconstants: %s",op[i].op.sphere.constants->name);
  	    }
  	  if (op[i].op.sphere.cs != NULL)
  	    {
  	      printf("\tcs: %s",op[i].op.sphere.cs->name);
  	    }
  	  add_sphere(tmp, op[i].op.sphere.d[0],
  		     op[i].op.sphere.d[1],
  		     op[i].op.sphere.d[2],
  		     op[i].op.sphere.r, step);
  	  matrix_mult( peek(systems), tmp );
  	  draw_polygons(tmp, t, g);
  	  tmp->lastcol = 0;
  	  break;
  	case TORUS:
  	  if (op[i].op.torus.constants != NULL)
  	    {
  	      printf("\tconstants: %s",op[i].op.torus.constants->name);
  	    }
  	  if (op[i].op.torus.cs != NULL)
  	    {
  	      printf("\tcs: %s",op[i].op.torus.cs->name);
  	    }
  	  add_torus(tmp,
  		    op[i].op.torus.d[0],
  		    op[i].op.torus.d[1],
  		    op[i].op.torus.d[2],
  		    op[i].op.torus.r0,op[i].op.torus.r1, step);
  	  matrix_mult( peek(systems), tmp );
  	  draw_polygons(tmp, t, g);
  	  tmp->lastcol = 0;
  	  break;
  	case BOX:
  	  if (op[i].op.box.constants != NULL)
  	    {
  	      printf("\tconstants: %s",op[i].op.box.constants->name);
  	    }
  	  if (op[i].op.box.cs != NULL)
  	    {
  	      printf("\tcs: %s",op[i].op.box.cs->name);
  	    }
  	  add_box(tmp,
  		  op[i].op.box.d0[0],op[i].op.box.d0[1],
  		  op[i].op.box.d0[2],
  		  op[i].op.box.d1[0],op[i].op.box.d1[1],
  		  op[i].op.box.d1[2]);
  	  matrix_mult(peek(systems), tmp );
  	  draw_polygons(tmp, t, g);
  	  tmp->lastcol = 0;
  	  break;
  	case LINE:
  	  if (op[i].op.line.constants != NULL)
  	    {
  	      printf("\n\tConstants: %s",op[i].op.line.constants->name);
  	    }
  	  if (op[i].op.line.cs0 != NULL)
  	    {
  	      printf("\n\tCS0: %s",op[i].op.line.cs0->name);
  	    }
  	  if (op[i].op.line.cs1 != NULL)
  	    {
  	      printf("\n\tCS1: %s",op[i].op.line.cs1->name);
  	    }
  	  break;
  	case MOVE:
      xval = op[i].op.move.d[0];
      yval = op[i].op.move.d[1];
      zval = op[i].op.move.d[2];
  	  if (op[i].op.move.p != NULL)
  	    {
          kval = op[i].op.move.p -> s.value;
  				xval *= kval;
  				yval *= kval;
  				zval *= kval;
  	    }
  	  tmp = make_translate(xval,yval,zval);
  	  matrix_mult(peek(systems), tmp);
  	  copy_matrix(tmp, peek(systems));
  	  tmp->lastcol = 0;
  	  break;
  	case SCALE:
      xval = op[i].op.scale.d[0];
      yval = op[i].op.scale.d[1];
      zval = op[i].op.scale.d[2];
      if (op[i].op.scale.p != NULL)
        {
        kval = op[i].op.scale.p -> s.value;
        xval *= kval;
        yval *= kval;
        zval *= kval;
      }
      tmp = make_scale(xval,yval,zval);
  	  matrix_mult(peek(systems), tmp);
  	  copy_matrix(tmp, peek(systems));
  	  tmp->lastcol = 0;
  	  break;
  	case ROTATE:
      theta =  op[i].op.rotate.degrees * (M_PI / 180);
  	  if (op[i].op.rotate.p != NULL)
  	    {
  	      kval = op[i].op.rotate.p -> s.value;
          theta *=kval;
  	    }
  	  if (op[i].op.rotate.axis == 0 )
  	    tmp = make_rotX( theta );
  	  else if (op[i].op.rotate.axis == 1 )
  	    tmp = make_rotY( theta );
  	  else
  	    tmp = make_rotZ( theta );

  	  matrix_mult(peek(systems), tmp);
  	  copy_matrix(tmp, peek(systems));
  	  tmp->lastcol = 0;
  	  break;
  	case PUSH:
  	  //printf("Push");
  	  push(systems);
  	  break;
  	case POP:
  	  //printf("Pop");
  	  pop(systems);
  	  break;
  	case SAVE:
  	  //printf("Save: %s",op[i].op.save.p->name);
  	  save_extension(t, op[i].op.save.p->name);
  	  break;
  	case DISPLAY:
  	 // printf("Display");
  	  display(t);
  	  break;
    case SET:
      set_value(lookup_symbol(op[i].op.set.p -> name), op[i].op.set.val);
      break;
    case SETKNOBS:
      for (k = 0; k < lastsym; k++)
        set_value(&symtab[i],op[i].op.setknobs.value);
      break;
    }
  }
  if(num_frames>1){
    char file[128];
    sprintf(file,"anim/%s%03d.png",name,j);
    save_extension(t,file);
    clear_screen(t);
    while(systems->top)pop(systems);
  }
  }
  make_animation(name);
    }
