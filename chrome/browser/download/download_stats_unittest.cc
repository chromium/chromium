// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_prompt_status.h"
#include "components/download/public/common/download_content.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kDownloadCancelReasonHistogram[] = "Download.CancelReason";

#if BUILDFLAG(IS_ANDROID)
constexpr char kDownloadPromptStatusHistogram[] =
    "MobileDownload.DownloadPromptStatus";

TEST(DownloadStatsTest, RecordDownloadPromptStatus) {
  base::HistogramTester histogram_tester;
  RecordDownloadPromptStatus(DownloadPromptStatus::SHOW_INITIAL);
  histogram_tester.ExpectBucketCount(kDownloadPromptStatusHistogram,
                                     DownloadPromptStatus::SHOW_INITIAL, 1);
  RecordDownloadPromptStatus(DownloadPromptStatus::SHOW_PREFERENCE);
  histogram_tester.ExpectBucketCount(kDownloadPromptStatusHistogram,
                                     DownloadPromptStatus::SHOW_PREFERENCE, 1);
  RecordDownloadPromptStatus(DownloadPromptStatus::DONT_SHOW);
  histogram_tester.ExpectBucketCount(kDownloadPromptStatusHistogram,
                                     DownloadPromptStatus::DONT_SHOW, 1);
  histogram_tester.ExpectTotalCount(kDownloadPromptStatusHistogram, 3);
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST(DownloadStatsTest, RecordDownloadCancelReason) {
  base::HistogramTester histogram_tester;
  RecordDownloadCancelReason(DownloadCancelReason::kTargetConfirmationResult);
  histogram_tester.ExpectBucketCount(
      kDownloadCancelReasonHistogram,
      DownloadCancelReason::kTargetConfirmationResult, 1);
  histogram_tester.ExpectTotalCount(kDownloadCancelReasonHistogram, 1);
}

TEST(DownloadStatsTest, RecordDownloadOpen) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  RecordDownloadOpen(DOWNLOAD_OPEN_METHOD_DEFAULT_BROWSER, "application/pdf");

  EXPECT_EQ(1, user_action_tester.GetActionCount("Download.Open"));
  histogram_tester.ExpectUniqueSample(
      "Download.OpenMethod",
      /*sample=*/DOWNLOAD_OPEN_METHOD_DEFAULT_BROWSER,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample("Download.Open.ContentType",
                                      /*sample=*/download::DownloadContent::PDF,
                                      /*expected_bucket_count=*/1);
}

}  // namespace
