//
//  Created by Adélie Gorce on 04/01/2018.
//  Copyright © 2018 Adélie GORCE. All rights reserved.
//

/*
 Executable file to compute the spherical correlation function
 as defined in Obreschkow et al., 2013, ApJ, 762:115
 from files with real and imaginary parts of Fourier tranform
 of matter overdensity in 2D box
 */

#include <vector>
#include <math.h>
#include <omp.h>
#include <algorithm>
#include <complex>
using namespace std;
#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <chrono>

#ifndef LENGTH
#define LENGTH SIZE
#endif
#ifndef NTHREADS
#define NTHREADS 10
#endif
const double pi=3.141592653589793;
const double s3=sqrt(3.0);
const int N=SIZE; //sampling number
const int NDIM=2; //dimension of the box
const double L=LENGTH; //length of the realspace box
const int nthreads = NTHREADS; //number of threads for parallelisation

static int total=0;

/* VERSION THAT DOES NOT COMPUTE THE IMAGINARY PART OF THE TRIANGLE CORRELATION FUNCTION */

const int min_int(int a, int b) {
    if (a>b) return b;
    else if (a <= b) return a;
    return 0;
}

vector<double> range(double min, double max, size_t N) {
    vector<double> range;
    double delta = (max-min)/double(N-1);
    for(int i=0; i<N; i++) {
        range.push_back(min + i*delta);
    }
    return range;
}

vector<double> k_axis() {
    vector<double> y;
    double delta = 2*pi/L;
    for (int i=0; i<floor(N*0.5); i++) {
        y.push_back(i*delta);
    }
    for (int i=0; i<floor(N*0.5); i++) {
        y.push_back(-1*delta*(floor(N*0.5)-i));
    }
    return y;
}

static double overdensity_kr[N][N], overdensity_ki[N][N];
static complex<double> overdensity_k[N][N], epsilon_k[N][N];
static vector<double> k_x=k_axis();
const double delta_k = 2.0*pi/L;

complex<double> sigma_plus(int i, int j, int i2, int j2, double r) {

    complex<double> B(0,0);
    int sx=0, sy=0;
    double norm_p=0, window;

    if (i==N | j==N | i2==N | j2==N) window=0; //in main, loops start at i=0, but there is no symmetrical term (sigma(N-i)) as it would have index N-i = N which does not exist in overdensity array: need to remove this contribution from final sum by saying it is zero
    else {
        vector<double> p(NDIM);
        p[0]=k_x[i]+k_x[i2]*0.5+s3*0.5*k_x[j2];
        p[1]=k_x[j]-s3*k_x[i2]*0.5+0.5*k_x[j2];
        sx=i+i2; if (sx>N-1) sx-=N;
        sy=j+j2; if (sy>N-1) sy-=N;
        // bispectrum
        B=epsilon_k[i2][j2]*epsilon_k[i][j]*conj(epsilon_k[sx][sy]);
        // p vector
        norm_p=sqrt(pow(p[0],2)+pow(p[1],2));
	//window function
        window=jn(0,norm_p*r);//Bessel function of the first kind order zero
        if (i==floor(N*0.5) | j==floor(N*0.5) | i2==floor(N*0.5) | j2==floor(N*0.5)) B*=0.5; //N/2th term will be counted 2 times because of symmetries in main function so need to divide value by 2
    }
    return B*window;
}

complex<double> add_sigma(int i, int j, int i2, int j2, double r) {

    complex<double> sigma(0,0);                 
    sigma+=sigma_plus(i, j, i2, j2, r);

    sigma+=sigma_plus(N-i, j, i2, j2, r);
    sigma+=sigma_plus(N-i, N-j, i2, j2, r);
    sigma+=sigma_plus(N-i, j,N- i2, j2, r);
    sigma+=sigma_plus(N-i, j, i2, N-j2, r);
    sigma+=sigma_plus(N-i, N-j, N-i2, j2, r);
    sigma+=sigma_plus(N-i, N-j, i2,N- j2, r);
    sigma+=sigma_plus(N-i, j, N-i2, N-j2, r);

    sigma+=sigma_plus(i, N-j, i2, j2, r);
    sigma+=sigma_plus(i, N-j,N-i2, j2, r);
    sigma+=sigma_plus(i, N-j, i2,N- j2, r);
    sigma+=sigma_plus(i, N-j, N-i2, N-j2, r);

    sigma+=sigma_plus(i, j, N-i2, j2, r);
    sigma+=sigma_plus(i, j, i2, N-j2, r);
    sigma+=sigma_plus(i, j, N-i2, N-j2, r);

    sigma+=sigma_plus(N-i, N-j, N-i2, N-j2, r);

    return sigma;

}

complex<double> sum_sigma(double r) {
// double sum_sigma(double r) {
 
    // Initialisation
    //int max_loop=min_int(floor(N*0.5),floor(pi/(r*delta_k))); //max value of loop index : k,q <= pi/r condition
    double sigma_real=0,sigma_imag=0;
    complex<double> sigma=(0,0);
    
    vector<int> v(NDIM);
    int u=1;
    double *k_norm, kni=0;
    vector< vector<int> > k_ind;
    k_norm=(double *) malloc(1*sizeof(double));
    k_norm = (double*) realloc(k_norm,1*sizeof(double));
    k_norm[0]=0;
    v.assign({0,0});
    k_ind.push_back(v);
    v.clear();
    for (int i=0; i<floor(N*0.5); i++) {
        for (int j=0; j<=i; j++) {
            kni=sqrt(pow(k_x[i],2)+pow(k_x[j],2));
            if ((kni<=pi/r) && (kni!=0)) {
                k_norm = (double*) realloc(k_norm,(u+1)*sizeof(double));
                k_norm[u]=kni; 
                v.assign({i,j});
                k_ind.push_back(v);
                //cout << u << " " << i << " " << j << " " << kni << " " << k_norm[u] << " " << v[0] << " " << v[1] <<endl;
                v.clear();
                u++;}
        }
    }
    int ksize=u;
    total+=ksize;
    cout << ksize << " / " <<flush;

    omp_set_num_threads(nthreads);
    #pragma omp parallel 
    {

    double norm_k=0, norm_q=0;
    vector<double> k(NDIM), q(NDIM);

    #pragma omp for reduction (+: sigma_real)
    for (int i=0; i<ksize; i++) {
        for (int j=0; j<ksize; j++) {
            if ( k_norm[i]<= pi/r
            && k_norm[j]<= pi/r ) {
            //&& sqrt(pow(k_x[k_ind[i][0]]+k_x[k_ind[j][0]],2)+pow(k_x[k_ind[i][1]]+k_x[k_ind[j][1]],2) ) <= pi/r) {
                sigma_real+= real(add_sigma(k_ind[i][0],k_ind[i][1],k_ind[j][0],k_ind[j][1],r));
                sigma_real+= real(add_sigma(k_ind[i][1],k_ind[i][0],k_ind[j][0],k_ind[j][1],r));
                sigma_real+= real(add_sigma(k_ind[i][0],k_ind[i][1],k_ind[j][1],k_ind[j][0],r));
                sigma_real+= real(add_sigma(k_ind[i][1],k_ind[i][0],k_ind[j][1],k_ind[j][0],r));
            }
        }
    }
    
    sigma_imag=0;
	    
    }//end of parallelisation

    sigma=complex<double>(sigma_real,sigma_imag);

    return sigma*pow(r/L,NDIM*1.5);

}


int main() {
  
    cout << "Dim in Fourier space " << N << " ; real space length " << L << " ; number of threads " << nthreads <<endl; 

    string real_file=string(filename)+string("_realpart.txt");
    string imag_file=string(filename)+string("_imagpart.txt");

    //Reading overdensity files
    ifstream overdensity_file_r, overdensity_file_i ;
    overdensity_file_r.open(real_file);
    overdensity_file_i.open(imag_file);
    if (!overdensity_file_r.is_open() | !overdensity_file_i.is_open() ) {cerr << "Unable to open file" <<endl; return -1;}
    cout << "Overdensity files opened: reading..." <<endl ;
    for (int i=0; i<N; i++) {
        for (int j=0; j<N; j++) {
            overdensity_file_r >> overdensity_kr[i][j];
            overdensity_file_i >> overdensity_ki[i][j];
            overdensity_k[i][j] = complex<double>(overdensity_kr[i][j] , overdensity_ki[i][j]);
            if (abs(overdensity_k[i][j])<delta_k) epsilon_k[i][j]=0;
            else epsilon_k[i][j] = overdensity_k[i][j]/abs(overdensity_k[i][j]);
        }
    }

    cout << "Done reading files, starting to loop" <<endl;
    overdensity_file_r.close();
    overdensity_file_i.close();


    string outfilename = string(filename)+string("_L")+to_string(int(L))+string("_spherical_correlations.txt");
    ofstream outfile;
    outfile.open(outfilename);
    outfile << "# dim " << N << endl;
    outfile << "# r Re[s(r)] Im[s(r)]" <<endl;

    int nbins=500;
    vector<double> r=range(0.5,50.,nbins);
    complex<double> l=(0,0);
    auto start = chrono::steady_clock::now();
    //complex<double> l(0,0);]
    cout << " r / size of k array / s(r)"  <<endl;
    for (int i=0; i<nbins; i++) {
        cout << r[i] << " / " <<flush;
        l=sum_sigma(r[i]);
        outfile << r[i] << " " << real(l) << " " << imag(l) <<endl;//<< real(l) << " " << imag(l) <<endl;
        cout << l << "     " <<endl;
    }
    outfile.close();

    cout << "Total number of steps: " << total<<endl;
    auto end = chrono::steady_clock::now();
    auto howlong = end - start;
    cout << "Executing time: " << chrono::duration <double> (howlong).count()/60 << "min" <<endl;

    ofstream arrayfile;
    arrayfile.open("array_sizes.txt",fstream::app);
    arrayfile << "NDIM=" << NDIM << " N=" << N << " L=" << L<< " total ksize = " << total <<endl;
    arrayfile.close();

    ofstream timefile;
    timefile.open("executing_times.txt", fstream::app);
    timefile << "NDIM=" << NDIM << " N=" << N << " L=" << L << " threads: " << nthreads << " time = " << chrono::duration <double> (howlong).count()/60 << "min" <<endl;
    timefile.close();

    
    return 0;
}