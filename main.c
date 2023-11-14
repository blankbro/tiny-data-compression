//
//  main.c
//  high-speed lossless tiny data compression for 1 to 512 bytes based on td512
//
//  file-based test bed outputs .td512 file with encoded values
//  then reads in that file and generates .td512d file with
//  original values.
//
//  Created by L. Stevan Leonard on 10/31/21.
//  Copyright © 2021-2022 L. Stevan Leonard. All rights reserved.
/*
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "td512.h" // td512 functions

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>

#define BENCHMARK_LOOP_COUNT // special loop count for benchmarking
#define EXTERNAL_LOOP_COUNT_MAX 2000
//#define TEST_TD512 // invokes test_td512_1to512

#ifdef TD512_TEST_MODE
extern uint32_t gExtendedStringCnt;
extern uint32_t gExtendedTextCnt;
extern uint32_t gtd64Cnt;
#endif

int32_t test_td512_1to512(void)
{
    // generate data then run through compress and decompress and compare for 1 to 512 values
    unsigned char textData[512]={"it over afterwards, it occurred to her that she ought to have wondered at this, but at the time it all seemed quite natural); but when the Rabbit actually TOOK A WATCH OUT OF ITS WAISTCOAT- POCKET, and looked at it, and then hurried on, Alice started to her feet, for it flashed across her mind that she had never before seen a rabbit with either a waistcoat-pocket, or a watch to take out of it, and burning with curiosity, she ran across the field after it, and fortunately was just in time to see it positive"};
    unsigned char textOut[515];
    unsigned char textOrig[512];
    uint32_t bytesProcessed;
    int32_t retVal;
    int i;
    int j;
RUN_512:
    for (i=1; i<=512; i++)
    {
        retVal = td512(textData, textOut, i);
        if (retVal < 0)
            return i;
        retVal = td512d(textOut, textOrig, &bytesProcessed);
        if (retVal < 0)
            return -i;
        for (j=0; j<i; j++)
        {
            if (textData[j] != textOrig[j])
                return 1000+j;
            textOut[j] = '0';
            textOrig[j] = '1';
        }
    }
    if (textData[0] == 'i')
    {
        // set all values to same value and run again
        memset(textData, 0x91, sizeof(textData));
        goto RUN_512;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    FILE *ifile, *ofile;
    char ofileName[FILENAME_MAX];
    unsigned char *src, *dst;
    size_t len, len2, len3;
    clock_t begin, end;
    double timeSpent;
    double minTimeSpent=600;
    int32_t nBytesRemaining;
    int32_t nCompressedBytes;
    uint32_t totalCompressedBytes;
    uint32_t bytesProcessed;
    uint32_t totalOutBytes;
    uint32_t srcBlockOffset;
    uint32_t dstBlockOffset;
    int loopNum;
    int loopCnt; // argv[4] option: default is 1
    uint32_t blockSize=512; // block size to use when iterating through file
    
    printf("tiny data compression td512 %s\n", TD512_VERSION);
#ifdef TEST_TD512
    int32_t retVal;
    if ((retVal=test_td512_1to512()) != 0) // do check of 1 to 512 values
    {
        printf("error from test_td512_1to512=%d\n", retVal);
        return -83;
    }
    printf("TEST_TD512 passed\n");
#endif
    if (argc < 2)
    {
        printf("td512 error: input file must be specified\n");
        return 14;
    }
    ifile = fopen(argv[1], "rb");
    if (!ifile)
    {
        printf("td512 error: file not found: %s\n", argv[1]);
        return 9;
    }
    printf("   file=%s\n", argv[1]);
    strcpy(ofileName, argv[1]);
    ofile = fopen(strcat(ofileName, ".td512"), "wb");

    // allocate source buffer and read file
    fseek(ifile, 0, SEEK_END);
    len = (size_t)ftell(ifile);
    fseek(ifile, 0, SEEK_SET);
    src = (unsigned char*) malloc(len);
    fread(src, 1, len, ifile);
    fclose(ifile);

    // allocate "uncompressed size" + 3 bytes per block for the destination buffer
    dst = (unsigned char*) malloc(len + 4 * (len / blockSize + 1));
    if (argc >= 3)
    {
        sscanf(argv[2], "%d", &loopCnt);
        if (loopCnt < 1)
            loopCnt = 1;
    }
    else
    {
        loopCnt = 1;
    }
#ifdef BENCHMARK_LOOP_COUNT // special loop count for benchmarking
    loopCnt = 100000000 / len;
    loopCnt = (loopCnt < 20) ? 20 : loopCnt;
#endif
    loopNum = 0;
    uint32_t internalLoop;
    internalLoop = 1;
    uint32_t savedInternalLoopCnt = 1;
    if (loopCnt > EXTERNAL_LOOP_COUNT_MAX)
    {
        savedInternalLoopCnt = loopCnt/EXTERNAL_LOOP_COUNT_MAX;
        loopCnt = EXTERNAL_LOOP_COUNT_MAX;
    }
    printf("   block size= %d   loop count= %d*%d\n", blockSize, loopCnt, savedInternalLoopCnt);

COMPRESS_LOOP:
    internalLoop = savedInternalLoopCnt;
    nBytesRemaining=(int32_t)len;
    totalCompressedBytes=0;
    srcBlockOffset=0;
    dstBlockOffset=0;

    // compress and write result
    begin = clock();
    struct timeval my_start, my_end;
    gettimeofday(&my_start, NULL);
    while (internalLoop > 0)
    {
        while (nBytesRemaining > 0)
        {
            uint32_t nBlockBytes=(uint32_t)nBytesRemaining>=blockSize ? blockSize : (uint32_t)nBytesRemaining;
            nCompressedBytes = td512(src+srcBlockOffset, dst+dstBlockOffset, nBlockBytes);
            if (nCompressedBytes < 0)
                exit(nCompressedBytes); // error occurred
            nBytesRemaining -= nBlockBytes;
            totalCompressedBytes += (uint32_t)nCompressedBytes;
            srcBlockOffset += nBlockBytes;
            dstBlockOffset += (uint32_t)nCompressedBytes;
        }
        if (--internalLoop > 0)
        {
            // perform some loops within the timing structure
            totalCompressedBytes = 0;
            nBytesRemaining=(int32_t)len;
            srcBlockOffset=0;
            dstBlockOffset=0;
        }
    }
    gettimeofday(&my_end, NULL);
    double my_time_used = (my_end.tv_sec - my_start.tv_sec) * 1000 + (my_end.tv_usec - my_start.tv_usec) / 1000.0;
    printf("压缩耗时: %f ms\n", my_time_used);
    end = clock();
    timeSpent = (double)(end - begin) / (double)CLOCKS_PER_SEC;
    if (timeSpent < minTimeSpent && timeSpent > 1.e-10)
        minTimeSpent = timeSpent;
    // if (++loopNum < loopCnt)
    // {
    //     usleep(10); // sleep 10 us
    //     goto COMPRESS_LOOP;
    // }
    timeSpent = minTimeSpent / savedInternalLoopCnt;
    printf("compression=%.02f%%  %.00f bytes per second inbytes=%lu outbytes=%u\n", (float)100*(1.0-((float)totalCompressedBytes/(float)len)), (float)len/(float)timeSpent, len, totalCompressedBytes);
#ifdef TD512_TEST_MODE
    double totalBlocks=gExtendedTextCnt+gExtendedStringCnt+gtd64Cnt;
    printf("TD512_TEST_MODE\n   Extended text mode=%.01f%%   Extended string mode= %.01f%%   td64 =%.01f%%\n", (float)gExtendedTextCnt/totalBlocks*100, (float)gExtendedStringCnt/totalBlocks*100, (float)gtd64Cnt/totalBlocks*100);
#endif

    fwrite(dst, totalCompressedBytes, 1, ofile);
    fclose(ofile);
    free(src);
    free(dst);
    
    // **********************
    // decompress
    ifile = fopen(ofileName, "rb");
    strcpy(ofileName, argv[1]);
    ofile = fopen(strcat(ofileName, ".td512d"), "wb");

    // allocate source buffer
    fseek(ifile, 0, SEEK_END);
    len3 = (size_t)ftell(ifile);
    fseek(ifile, 0, SEEK_SET);
    src = (unsigned char*) malloc(len3);
    
    // read file and allocate destination buffer
    fread(src, 1, len3, ifile);
    len2 = len; // output==input
    dst = (unsigned char*) malloc(len2);
    fclose(ifile);
    
    minTimeSpent=600;
    loopNum = 0;
DECOMPRESS_LOOP:
    // decompress and write result
    internalLoop = savedInternalLoopCnt;
    totalOutBytes = 0;
    nBytesRemaining = (int32_t)len3;
    srcBlockOffset = 0;
    dstBlockOffset = 0;
    begin = clock();
    gettimeofday(&my_start, NULL);
    while (internalLoop > 0)
    {
        while (nBytesRemaining > 0)
        {
            int32_t nRetBytes;
            nRetBytes = td512d(src+srcBlockOffset, dst+dstBlockOffset, &bytesProcessed);
            if (nRetBytes < 0)
                return nRetBytes;
            assert(nBytesRemaining>=blockSize?nRetBytes==blockSize:1);
            nBytesRemaining -= bytesProcessed;
            totalOutBytes += (uint32_t)nRetBytes;
            srcBlockOffset += bytesProcessed;
            dstBlockOffset += (uint32_t)nRetBytes;
        }
        if (--internalLoop > 0)
        {
            totalOutBytes = 0;
            nBytesRemaining = (int32_t)len3;
            srcBlockOffset = 0;
            dstBlockOffset = 0;
        }
    }
    gettimeofday(&my_end, NULL);
    my_time_used = (my_end.tv_sec - my_start.tv_sec) * 1000 + (my_end.tv_usec - my_start.tv_usec) / 1000.0;
    printf("解压耗时: %f ms\n", my_time_used);
    end = clock();
    timeSpent = (double)(end - begin) / (double)CLOCKS_PER_SEC;
    if (timeSpent < minTimeSpent && timeSpent > 1.e-10)
        minTimeSpent = timeSpent;
    // if (++loopNum < loopCnt)
    // {
    //     usleep(10); // sleep 10 us
    //     goto DECOMPRESS_LOOP;
    // }
    timeSpent = minTimeSpent / savedInternalLoopCnt;
    printf("decompression=%.00f bytes per second inbytes=%lu outbytes=%u\n", (float)len/(float)timeSpent, len3, totalOutBytes);
    fwrite(dst, len, 1, ofile);
    fclose(ofile);
    free(src);
    // verify original input file with decompressed output
    ifile = fopen(argv[1], "rb");
    if (!ifile)
    {
        printf("td512 error: file not found to verify with decompressed output file: %s\n", argv[1]);
        return 9;
    }
    // allocate source buffer and read file
    src = (unsigned char*) malloc(len);
    fread(src, 1, len, ifile);
    fclose(ifile);
    if ((memcmp(src, dst, len)) != 0)
    {
        printf("td512 error: decompressed file differs from original input file\n");
        free(src);
        free(dst);
        return 31;
    }
    free(src);
    free(dst);
    return 0;
}
