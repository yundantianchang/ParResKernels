/*
Copyright (c) 2013, Intel Corporation

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions 
are met:

* Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above 
      copyright notice, this list of conditions and the following 
      disclaimer in the documentation and/or other materials provided 
      with the distribution.
* Neither the name of Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products 
      derived from this software without specific prior written 
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
POSSIBILITY OF SUCH DAMAGE.
*/

/*******************************************************************

NAME:    transpose

PURPOSE: This program measures the time for the transpose of a 
         column-major stored matrix into a row-major stored matrix.
  
USAGE:   Program input is the matrix order and the number of times to 
         repeat the operation:

         transpose <matrix_size> <# iterations> [tile size]

         An optional parameter specifies the tile size used to divide the
         individual matrix blocks for improved cache and TLB performance. 
  
         The output consists of diagnostics to make sure the 
         transpose worked and timing statistics.


FUNCTIONS CALLED:

         Other than standard C functions, the following 
         functions are used in this program:

         wtime()          portable wall-timer interface.
         test_results()   Verify that the transpose worked

HISTORY: Written by  Rob Van der Wijngaart, February 2009.
*******************************************************************/

#include <par-res-kern_general.h>

/* Constant to shift column index */
#define COL_SHIFT 1000.00   
/* Constant to shift row index */
#define ROW_SHIFT  0.001    

static double test_results (int , double*);

int main(int argc, char ** argv) {

  int    order;         /* order of a the matrix                           */
  int    tile_size=32;  /* default tile size for tiling of local transpose */
  int    iterations;    /* number of times to do the transpose             */
  int    i, j, it, jt, iter;  /* dummies                                   */
  double bytes;         /* combined size of matrices                       */
  double * RESTRICT A;  /* buffer to hold original matrix                  */
  double * RESTRICT B;  /* buffer to hold transposed matrix                */
  double errsq;         /* squared error                                   */
  double epsilon=1.e-8; /* error tolerance                                 */
  double trans_time,    /* timing parameters                               */
         avgtime = 0.0, 
         maxtime = 0.0, 
         mintime = 366.0*24.0*3600.0; /* set the minimum time to a large 
                             value; one leap year should be enough         */

  /*********************************************************************
  ** read and test input parameters
  *********************************************************************/

  if (argc != 4 && argc != 3){
    printf("Usage: %s <# iterations> <matrix order> [tile size]\n",
           *argv);
    exit(EXIT_FAILURE);
  }

  iterations  = atoi(*++argv); 
  if (iterations < 1){
    printf("ERROR: iterations must be >= 1 : %d \n",iterations);
    exit(EXIT_FAILURE);
  }

  order = atoi(*++argv); 
  if (order < 0){
    printf("ERROR: Matrix Order must be greater than 0 : %d \n", order);
    exit(EXIT_FAILURE);
  }

  if (argc == 4) tile_size = atoi(*++argv);
  /* a non-positive tile size means no tiling of the local transpose */
  if (tile_size <=0) tile_size = order;

  /*********************************************************************
  ** Allocate space for the input and transpose matrix
  *********************************************************************/

  A   = (double *)malloc(order*order*sizeof(double));
  if (A == NULL){
    printf(" Error allocating space for input matrix\n");
    exit(EXIT_FAILURE);
  }
  B  = (double *)malloc(order*order*sizeof(double));
  if (B == NULL){
    printf(" Error allocating space for transposed matrix\n");
    exit(EXIT_FAILURE);
  }

  bytes = 2.0 * sizeof(double) * order * order;

  printf("Serial Matrix transpose: B = A^T\n");
  printf("Matrix order          = %d\n", order);
  if (tile_size < order) printf("Tile size             = %d\n", tile_size);
  else                   printf("Untiled\n");
  printf("Number of iterations  = %d\n", iterations);

  /*  Fill the original matrix, set transpose to known garbage value. */

  for (i=0;i<order; i++) {
    for (j=0;j<order;j++) {
      *(A+i*order+j) = COL_SHIFT * j + ROW_SHIFT * i;
      *(B+i*order+j) = -1.0;
    }
  }

  errsq = 0.0;
  for (iter = 0; iter<iterations; iter++){

    trans_time = wtime();

    /* Transpose the  matrix; only use tiling if the tile size is smaller 
       than the matrix */
    if (tile_size < order) {
      for (i=0; i<order; i+=tile_size) { 
        for (j=0; j<order; j+=tile_size) { 
          for (it=i; it<MIN(order,i+tile_size); it++){ 
            for (jt=j; jt<MIN(order,j+tile_size);jt++){ 
              B[it+order*jt] = A[jt+order*it]; 
            } 
          } 
        } 
      } 
    }
    else {
      for (i=0;i<order; i++) {
        for (j=0;j<order;j++) {
          B[i+order*j] = A[j+order*i];
        }
      }
    }	

    trans_time = wtime() - trans_time;
#ifdef VERBOSE
    printf("\nFinished with transpose, using %lf seconds \n", trans_time);
#endif
    if (iter>0 || iterations==1) { /* skip the first iteration */
      avgtime = avgtime + trans_time;
      mintime = MIN(mintime, trans_time);
      maxtime = MAX(maxtime, trans_time);
    }

    errsq +=  test_results (order, B);

  }  /* end of iter loop  */

  /*********************************************************************
  ** Analyze and output results.
  *********************************************************************/

  if (errsq < epsilon) {
    printf("Solution validates\n");
    avgtime = avgtime/(double)(MAX(iterations-1,1));
    printf("Rate (MB/s): %lf, Avg time (s): %lf, Min time (s): %lf",
           1.0E-06 * bytes/mintime, avgtime, mintime);
    printf(", Max time (s): %lf\n", maxtime);
#ifdef VERBOSE
    printf("Squared errors: %f \n", errsq);
#endif
    exit(EXIT_SUCCESS);
  }
  else {
    printf("ERROR: Aggregate squared error %lf exceeds threshold %e\n",
           errsq, epsilon);
    exit(EXIT_FAILURE);
  }

}  /* end of main */



/* function that computes the error committed during the transposition */

double test_results (int order, double *trans) {

  double diff, errsq=0.0;
  int i,j;

  for (i=0;i<order; i++) {
    for (j=0;j<order;j++) {
      diff = *(trans+i*order+j) -
             (COL_SHIFT*i  + ROW_SHIFT * j);
      errsq += diff*diff;
    }
  }

#ifdef VERBOSE
  printf(" Squared sum of differences: %f\n",errsq);
#endif   
  return errsq;
}
