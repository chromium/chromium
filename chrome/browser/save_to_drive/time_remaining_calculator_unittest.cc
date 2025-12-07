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
       CalculateTimeRemainingTextWithNoAdditionalBytesUploaded) {
  pdf_api::SaveToDriveProgress progress = CreateUploadStartedProgress();
  progress.status = pdf_api::SaveToDriveStatus::kUploadInProgress;
  EXPECT_EQ(calculator().CalculateTimeRemainingText(progress), std::nullopt);

  FastForwardBy(base::Seconds(10));
  EXPECT_EQ(calculator().CalculateTimeRemainingText(progress), std::nullopt);

  FastForwardBy(base::Minutes(10));
  EXPECT_EQ(calculator().CalculateTimeRemainingText(progress), std::nullopt);
}

TEST_F(SaveToDriveTimeRemainingCalculatorTest,
       CalculateTimeRemainingTextAfterTimePassed) {
  pdf_api::SaveToDriveProgress progress = CreateUploadStartedProgress();
  progress.status = pdf_api::SaveToDriveStatus::kUploadInProgress;
  EXPECT_EQ(calculator().CalculateTimeRemainingText(progress), std::nullopt);

  // Speed estimate: 1 MiB/s. Remaining: 90 MiB.
  // Time: 90 MiB / 1 MiB/s = 90s.
  progress.uploaded_bytes = 10 * 1024 * 1024;
  FastForwardBy(base::Seconds(10));
  EXPECT_THAT(calculator().CalculateTimeRemainingText(progress),
              testing::Optional(std::u16string_view(u"2 minutes left")));

  // Speed estimate: 1 MiB/s. Remaining: 80 MiB.
  // Time: 80 MiB / 1 MiB/s = 80s.
  progress.uploaded_bytes = 20 * 1024 * 1024;
  FastForwardBy(base::Seconds(10));
  EXPECT_THAT(calculator().CalculateTimeRemainingText(progress),
              testing::Optional(std::u16string_view(u"1 minute left")));

  // Speed estimate: 0 MiB/s.
  // Time: no speed output, so no estimate.
  FastForwardBy(base::Seconds(10));
  EXPECT_THAT(calculator().CalculateTimeRemainingText(progress), std::nullopt);

  // Speed estimate: 3 MiB/s. Remaining: 50 MiB.
  // Time: 50 MiB / 3 MiB/s = 16.67s.
  progress.uploaded_bytes = 50 * 1024 * 1024;
  FastForwardBy(base::Seconds(10));
  EXPECT_THAT(calculator().CalculateTimeRemainingText(progress),
              testing::Optional(std::u16string_view(u"16 seconds left")));

  // Speed estimate: 0.1 MiB/s. Remaining: 49 MiB.
  // Time: 49 MiB / 0.1 MiB/s = 490s (8.16 minutes).
  progress.uploaded_bytes = 51 * 1024 * 1024;
  FastForwardBy(base::Seconds(10));
  EXPECT_THAT(calculator().CalculateTimeRemainingText(progress),
              testing::Optional(std::u16string_view(u"8 minutes left")));

  // Speed estimate: 4 MiB/s. Remaining: 45 MiB.
  // Time: 45/4 = 11.25s.
  progress.uploaded_bytes = 55 * 1024 * 1024;
  FastForwardBy(base::Seconds(15));
  EXPECT_THAT(calculator().CalculateTimeRemainingText(progress),
              testing::Optional(std::u16string_view(u"11 seconds left")));

  // Speed estimate: 1 MiB/s. Remaining: 44 MiB.
  // Time: 44/1 = 44s.
  progress.uploaded_bytes = 56 * 1024 * 1024;
  FastForwardBy(base::Seconds(20));
  EXPECT_THAT(calculator().CalculateTimeRemainingText(progress),
              testing::Optional(std::u16string_view(u"44 seconds left")));
}

}  // namespace
}  // namespace save_to_drive
