// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_browsertest_perf.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace test {

constexpr char kTypicalCompareVideosOutput[] =
    "Adjusting test video with color transformation: \n"
    " 1.00   0.01   0.01  -2.91 \n"
    "-0.01   0.99  -0.04   7.21 \n"
    " 0.00  -0.06   0.99   7.47 \n"
    "RESULT Unique_frames_count: 360p_VP8= 80 score\n"
    "RESULT PSNR: 360p_VP8= "
    "[38.453419,38.453419,38.453419,38.453419,38.453419,38.453419,38.453419,38."
    "453419,38.453419,38.453419,39.102536,39.102536,39.102536,39.767288,39."
    "767288,39.767288,40.023144,40.023144,40.023144,40.562812,40.562812,40."
    "562812,40.72701,40.72701,40.72701,40.72701,40.926442,40.926442,40.926442,"
    "41.198192,41.198192,41.198192,41.397378,41.397378,41.397378,41.435832,41."
    "435832,41.435832,41.456998,41.456998,41.456998,41.66108,41.66108,41.66108,"
    "41.722977,41.722977,41.722977,41.471985,41.471985,41.471985,41.471985,41."
    "471985,41.263275,41.263275,41.263275,40.953795,40.953795,40.890606,40."
    "890606,40.890606,41.055124,41.055124,41.055124,41.371183,41.371183,41."
    "371183,41.64044,41.64044,41.64044,41.64044,41.64044,41.725886,41.725886,"
    "41.725886,41.578544,41.578544,41.646766,41.646766,41.490909,41.490909] "
    "dB\n"
    "RESULT SSIM: 360p_VP8= "
    "[0.96503067,0.96503067,0.96503067,0.96503067,0.96503067,0.96503067,0."
    "96503067,0.96503067,0.96503067,0.96503067,0.96694655,0.96694655,0."
    "96694655,0.97058175,0.97058175,0.97058175,0.97440174,0.97440174,0."
    "97440174,0.97723814,0.97723814,0.97723814,0.97804682,0.97804682,0."
    "97804682,0.97804682,0.98044036,0.98044036,0.98044036,0.98102023,0."
    "98102023,0.98102023,0.98076329,0.98076329,0.98076329,0.98025288,0."
    "98025288,0.98025288,0.98084894,0.98084894,0.98084894,0.98137786,0."
    "98137786,0.98137786,0.9812953,0.9812953,0.9812953,0.97990543,0.97990543,0."
    "97990543,0.97990543,0.97990543,0.97811092,0.97811092,0.97811092,0."
    "97576317,0.97576317,0.97655883,0.97655883,0.97655883,0.97669573,0."
    "97669573,0.97669573,0.9795819,0.9795819,0.9795819,0.98144956,0.98144956,0."
    "98144956,0.98144956,0.98144956,0.98165894,0.98165894,0.98165894,0."
    "98185588,0.98185588,0.98135814,0.98135814,0.98102463,0.98102463] score\n"
    "RESULT Max_repeated: 360p_VP8= 10\n"
    "RESULT Max_skipped: 360p_VP8= 1\n"
    "RESULT Total_skipped: 360p_VP8= 23\n"
    "RESULT Decode_errors_reference: 360p_VP8= 0\n"
    "RESULT Decode_errors_test: 360p_VP8= 0\n";

TEST(WebrtcBrowserTestPerfTest, ParsesTypicalCompareVideosOutput) {
  EXPECT_TRUE(WriteCompareVideosOutputAsHistogram("someLabel",
                                                  kTypicalCompareVideosOutput));
}

TEST(WebrtcBrowserTestPerfTest, FailsOnWrongNumberOfLines) {
  EXPECT_FALSE(WriteCompareVideosOutputAsHistogram(
      "whatever", "RESULT bad_label: 360p_VP8= 80 score\n"));
}

TEST(WebrtcBrowserTestPerfTest, FailsOnBadLabels) {
  EXPECT_FALSE(WriteCompareVideosOutputAsHistogram(
      "whatever", "RESULT bad_label: 360p_VP8= 80 score\na\nb\nc\nd\ne\nf\ng"));
}

TEST(WebrtcBrowserTestPerfTest, FailsOnBadValues) {
  EXPECT_FALSE(WriteCompareVideosOutputAsHistogram(
      "whatever",
      "RESULT bad_label: 360p_VP8= meh score\na\nb\nc\nd\ne\nf\ng"));
}

TEST(WebrtcBrowserTestPerfTest, FailsIfLabelsInWrongOrder) {
  EXPECT_FALSE(WriteCompareVideosOutputAsHistogram(
      "whatever", "RESULT PSNR: 360p_VP8= 80 score\na\nb\nc\nd\ne\nf\ng"));
}

}  // namespace test
