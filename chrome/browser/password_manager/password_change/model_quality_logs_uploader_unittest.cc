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
using QualityStatus = optimization_guide::proto::
    PasswordChangeQuality_StepQuality_SubmissionStatus;
using PasswordChangeSubmissionLoggingData =
    optimization_guide::proto::PasswordChangeSubmissionLoggingData;
using PasswordChangeOutcome = ::optimization_guide::proto::
    PasswordChangeSubmissionData_PasswordChangeOutcome;
using PageType = optimization_guide::proto::OpenFormResponseData_PageType;

namespace {
void CheckOpenFormStatus(const optimization_guide::proto::LogAiDataRequest& log,
                         const QualityStatus& expected_status) {
  EXPECT_EQ(log.password_change_submission().quality().open_form().status(),
            expected_status);
}

void CheckSubmitFormStatus(
    const optimization_guide::proto::LogAiDataRequest& log,
    const QualityStatus& expected_status) {
  EXPECT_EQ(log.password_change_submission().quality().submit_form().status(),
            expected_status);
}

void CheckVerifySubmissionStatus(
    const optimization_guide::proto::LogAiDataRequest& log,
    const QualityStatus& expected_status,
    const FinalModelStatus& expected_final_status) {
  EXPECT_EQ(log.password_change_submission().quality().final_model_status(),
            expected_final_status);
  EXPECT_EQ(
      log.password_change_submission().quality().verify_submission().status(),
      expected_status);
}
}  // namespace

class ModelQualityLogsUploaderTest : public ChromeRenderViewHostTestHarness {
 public:
  ModelQualityLogsUploaderTest() = default;
  ~ModelQualityLogsUploaderTest() override = default;
};

TEST_F(ModelQualityLogsUploaderTest, VerifySubmissionSucessLog) {
  ModelQualityLogsUploader logs_uploader(profile());
  auto logging_data = std::make_unique<PasswordChangeSubmissionLoggingData>();
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_outcome_data()->set_submission_outcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);
  logs_uploader.SetVerifySubmissionQuality(std::optional(response),
                                           std::move(logging_data));
  CheckVerifySubmissionStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      FinalModelStatus::FINAL_MODEL_STATUS_SUCCESS);
}

TEST_F(ModelQualityLogsUploaderTest, OpenFormSuccessLog) {
  ModelQualityLogsUploader logs_uploader(profile());
  auto logging_data = std::make_unique<PasswordChangeSubmissionLoggingData>();
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(response, std::move(logging_data));

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
}

TEST_F(ModelQualityLogsUploaderTest, OpenFormElementNotFoundLog) {
  ModelQualityLogsUploader logs_uploader(profile());
  auto logging_data = std::make_unique<PasswordChangeSubmissionLoggingData>();
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  logs_uploader.SetOpenFormQuality(response, std::move(logging_data));
  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_F(ModelQualityLogsUploaderTest, OpenFormUnexpectedStateLog) {
  ModelQualityLogsUploader logs_uploader(profile());
  auto logging_data = std::make_unique<PasswordChangeSubmissionLoggingData>();
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_LOG_IN_PAGE);
  logs_uploader.SetOpenFormQuality(response, std::move(logging_data));
  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE);
}

TEST_F(ModelQualityLogsUploaderTest, SubmitFormSuccessLog) {
  ModelQualityLogsUploader logs_uploader(profile());
  auto logging_data = std::make_unique<PasswordChangeSubmissionLoggingData>();
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_submit_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetSubmitFormQuality(response, std::move(logging_data));
  CheckSubmitFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
}

TEST_F(ModelQualityLogsUploaderTest, SubmitFormElementNotFoundLog) {
  ModelQualityLogsUploader logs_uploader(profile());
  auto logging_data = std::make_unique<PasswordChangeSubmissionLoggingData>();
  optimization_guide::proto::PasswordChangeResponse response;
  logs_uploader.SetSubmitFormQuality(response, std::move(logging_data));
  CheckSubmitFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_F(ModelQualityLogsUploaderTest, MergeLogsDoesNotOverwrite) {
  ModelQualityLogsUploader logs_uploader(profile());

  // Helper function to create a unique_ptr for logging data.
  auto CreateLoggingData = []() {
    return std::make_unique<PasswordChangeSubmissionLoggingData>();
  };

  // Set open form data.
  optimization_guide::proto::PasswordChangeResponse open_form_response;
  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response, CreateLoggingData());

  // Set submit form data.
  optimization_guide::proto::PasswordChangeResponse submit_form_response;
  submit_form_response.mutable_submit_form_data()->set_dom_node_id_to_click(
      123);
  logs_uploader.SetSubmitFormQuality(submit_form_response, CreateLoggingData());

  // Set verify submission data.
  optimization_guide::proto::PasswordChangeResponse verify_submission_response;
  logs_uploader.SetVerifySubmissionQuality(verify_submission_response,
                                           CreateLoggingData());

  // Verify all steps have quality data and it is not overwritten.
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();

  CheckOpenFormStatus(
      final_log,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  CheckSubmitFormStatus(
      final_log,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  CheckVerifySubmissionStatus(
      final_log,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      FinalModelStatus::FINAL_MODEL_STATUS_SUCCESS);
}
