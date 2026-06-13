// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prompt_status.h"
#include "components/download/public/common/mock_download_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::Return;
using ::testing::ReturnRefOfCopy;

namespace {

constexpr char kDownloadCancelReasonHistogram[] = "Download.CancelReason";

#if BUILDFLAG(IS_ANDROID)
constexpr char kDownloadPromptStatusHistogram[] =
    "MobileDownload.DownloadPromptStatus";
constexpr char kShowedDownloadWarningHistogram[] =
    "Download.ShowedDownloadWarning";

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

TEST(DownloadStatsTest, RecordDangerousDownloadWarningShownForSafeDownload) {
  // Initialize mocks.
  base::HistogramTester histogram_tester;
  NiceMock<download::MockDownloadItem> mock_download_item;
  std::unique_ptr<DownloadUIModel> download_ui_model =
      std::make_unique<DownloadItemModel>(&mock_download_item);
  EXPECT_FALSE(download_ui_model->WasUIWarningShown());

  // Mock expected behavior.
  base::FilePath target_path(FILE_PATH_LITERAL("/test.apk"));
  ON_CALL(mock_download_item, GetTargetFilePath())
      .WillByDefault(ReturnRefOfCopy(target_path));
  ON_CALL(mock_download_item, GetURL())
      .WillByDefault(ReturnRefOfCopy(GURL("https://chromium.org")));
  ON_CALL(mock_download_item, GetState())
      .WillByDefault(
          Return(download::DownloadItem::DownloadState::IN_PROGRESS));
  ON_CALL(mock_download_item, GetDangerType())
      .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));

  // Log metrics.
  MaybeRecordDangerousDownloadWarningShown(*download_ui_model);

  // Validate expected behavior.
  EXPECT_TRUE(download_ui_model->WasUIWarningShown());
  histogram_tester.ExpectUniqueSample(
      kShowedDownloadWarningHistogram,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, 1);

  // Metric logging is skipped because the warning was already shown.
  MaybeRecordDangerousDownloadWarningShown(*download_ui_model);

  // Verify that there is no overcounting.
  histogram_tester.ExpectUniqueSample(
      kShowedDownloadWarningHistogram,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, 1);
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
}

}  // namespace
