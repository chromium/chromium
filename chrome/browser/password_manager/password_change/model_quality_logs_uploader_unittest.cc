// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"

#include <memory>
#include <vector>

#include "base/test/test_future.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using FinalModelStatus = optimization_guide::proto::FinalModelStatus;
using PasswordChangeSubmissionLoggingData =
    optimization_guide::proto::PasswordChangeSubmissionLoggingData;
using PasswordChangeOutcome = ::optimization_guide::proto::
    PasswordChangeSubmissionData_PasswordChangeOutcome;

class ModelQualityLogsUploaderTest : public ChromeRenderViewHostTestHarness {
 public:
  ModelQualityLogsUploaderTest() = default;
  ~ModelQualityLogsUploaderTest() override = default;
};

TEST_F(ModelQualityLogsUploaderTest, AddFinalModelStatusLog) {
  ModelQualityLogsUploader logs_uploader(profile());
  auto logging_data = std::make_unique<PasswordChangeSubmissionLoggingData>();
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_outcome_data()->set_submission_outcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);
  logs_uploader.MergeData(response, std::move(logging_data));
  auto logs = logs_uploader.GetLogEntryRequestsForTesting();
  ASSERT_EQ(logs.size(), 1u);
  ASSERT_EQ(logs[0]
                .mutable_password_change_submission()
                ->mutable_quality()
                ->final_model_status(),
            FinalModelStatus::FINAL_MODEL_STATUS_SUCCESS);
}
