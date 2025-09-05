// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/time_remaining_calculator.h"

#include <string_view>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace save_to_drive {
namespace {

namespace pdf_api = extensions::api::pdf_viewer_private;

pdf_api::SaveToDriveProgress CreateUploadStartedProgress() {
  pdf_api::SaveToDriveProgress progress;
  progress.status = pdf_api::SaveToDriveStatus::kUploadStarted;
  progress.error_type = pdf_api::SaveToDriveErrorType::kNoError;
  progress.uploaded_bytes = 0;
  progress.file_size_bytes = 100 * 1024 * 1024;
  return progress;
}

class SaveToDriveTimeRemainingCalculatorTest : public testing::Test {
 protected:
  void FastForwardBy(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  TimeRemainingCalculator& calculator() { return calculator_; }

 private:
  // Must be the first member.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TimeRemainingCalculator calculator_;
};

TEST_F(SaveToDriveTimeRemainingCalculatorTest,
       CalculateTimeRemainingTextWithNoTimePassed) {
  pdf_api::SaveToDriveProgress progress = CreateUploadStartedProgress();
  EXPECT_EQ(calculator().CalculateTimeRemainingText(progress), std::nullopt);

  progress.status = pdf_api::SaveToDriveStatus::kUploadInProgress;
  progress.uploaded_bytes = 10 * 1024 * 1024;
  EXPECT_EQ(calculator().CalculateTimeRemainingText(progress), std::nullopt);

  progress.status = pdf_api::SaveToDriveStatus::kUploadInProgress;
  progress.uploaded_bytes = 50 * 1024 * 1024;
  EXPECT_EQ(calculator().CalculateTimeRemainingText(progress), std::nullopt);
}

TEST_F(SaveToDriveTimeRemainingCalculatorTest,
       CalculateTimeRemainingTextAfterTimePassed) {
  pdf_api::SaveToDriveProgress progress = CreateUploadStartedProgress();
  EXPECT_EQ(calculator().CalculateTimeRemainingText(progress), std::nullopt);

  progress.status = pdf_api::SaveToDriveStatus::kUploadInProgress;
  progress.uploaded_bytes = 10 * 1024 * 1024;
  FastForwardBy(base::Seconds(10));
  EXPECT_THAT(calculator().CalculateTimeRemainingText(progress),
              testing::Optional(std::u16string_view(u"2 minutes left")));

  progress.uploaded_bytes = 40 * 1024 * 1024;
  FastForwardBy(base::Seconds(6));
  EXPECT_THAT(calculator().CalculateTimeRemainingText(progress),
              testing::Optional(std::u16string_view(u"12 seconds left")));
}

TEST_F(SaveToDriveTimeRemainingCalculatorTest,
       CalculateTimeRemainingTextAfterUploadRestarted) {
  pdf_api::SaveToDriveProgress progress = CreateUploadStartedProgress();
  EXPECT_EQ(calculator().CalculateTimeRemainingText(progress), std::nullopt);

  progress.status = pdf_api::SaveToDriveStatus::kUploadInProgress;
  progress.uploaded_bytes = 10 * 1024 * 1024;
  FastForwardBy(base::Seconds(10));
  EXPECT_THAT(calculator().CalculateTimeRemainingText(progress),
              testing::Optional(std::u16string_view(u"2 minutes left")));

  progress.status = pdf_api::SaveToDriveStatus::kUploadStarted;
  progress.uploaded_bytes = 0;
  EXPECT_EQ(calculator().CalculateTimeRemainingText(progress), std::nullopt);

  progress.status = pdf_api::SaveToDriveStatus::kUploadInProgress;
  progress.uploaded_bytes = 30 * 1024 * 1024;
  FastForwardBy(base::Minutes(2));
  EXPECT_THAT(calculator().CalculateTimeRemainingText(progress),
              testing::Optional(std::u16string_view(u"5 minutes left")));
}

}  // namespace
}  // namespace save_to_drive
