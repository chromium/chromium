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

std::unique_ptr<optimization_guide::proto::PasswordChangeSubmissionLoggingData>
CreateLoggingData() {
  return std::make_unique<PasswordChangeSubmissionLoggingData>();
}
}  // namespace

class ModelQualityLogsUploaderTest : public ChromeRenderViewHostTestHarness {
 public:
  ModelQualityLogsUploaderTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ModelQualityLogsUploaderTest() override = default;
};

TEST_F(ModelQualityLogsUploaderTest, VerifySubmissionSucessLog) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(profile());
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_outcome_data()->set_submission_outcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);
  logs_uploader.SetVerifySubmissionQuality(
      std::optional(response), CreateLoggingData(), fake_start_time);
  CheckVerifySubmissionStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      FinalModelStatus::FINAL_MODEL_STATUS_SUCCESS);
}

TEST_F(ModelQualityLogsUploaderTest, OpenFormSuccessLog) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(profile());
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(response, CreateLoggingData(),
                                   fake_start_time);

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
}

TEST_F(ModelQualityLogsUploaderTest, OpenFormElementNotFoundLog) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(profile());
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  logs_uploader.SetOpenFormQuality(response, CreateLoggingData(),
                                   fake_start_time);
  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_F(ModelQualityLogsUploaderTest, OpenFormUnexpectedStateLog) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(profile());
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_LOG_IN_PAGE);
  logs_uploader.SetOpenFormQuality(response, CreateLoggingData(),
                                   fake_start_time);
  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE);
}

TEST_F(ModelQualityLogsUploaderTest, SubmitFormSuccessLog) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(profile());
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_submit_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetSubmitFormQuality(response, CreateLoggingData(),
                                     fake_start_time);
  CheckSubmitFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
}

TEST_F(ModelQualityLogsUploaderTest, SubmitFormElementNotFoundLog) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(profile());
  optimization_guide::proto::PasswordChangeResponse response;
  logs_uploader.SetSubmitFormQuality(response, CreateLoggingData(),
                                     fake_start_time);
  CheckSubmitFormStatus(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_F(ModelQualityLogsUploaderTest, MergeLogsDoesNotOverwrite) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(profile());
  // Set open form data.
  optimization_guide::proto::PasswordChangeResponse open_form_response;
  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response, CreateLoggingData(),
                                   fake_start_time);

  // Set submit form data.
  optimization_guide::proto::PasswordChangeResponse submit_form_response;
  submit_form_response.mutable_submit_form_data()->set_dom_node_id_to_click(
      123);
  logs_uploader.SetSubmitFormQuality(submit_form_response, CreateLoggingData(),
                                     fake_start_time);

  // Set verify submission data.
  optimization_guide::proto::PasswordChangeResponse verify_submission_response;
  logs_uploader.SetVerifySubmissionQuality(
      verify_submission_response, CreateLoggingData(), fake_start_time);

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

TEST_F(ModelQualityLogsUploaderTest, LatencyRecordedForAllSteps) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(profile());
  constexpr int64_t expected_latency_ms = 2;
  constexpr base::TimeDelta latency = base::Milliseconds(expected_latency_ms);

  task_environment()->FastForwardBy(latency);
  // Set open form data.
  optimization_guide::proto::PasswordChangeResponse open_form_response;
  logs_uploader.SetOpenFormQuality(open_form_response, CreateLoggingData(),
                                   fake_start_time);

  // Set submit form data.
  optimization_guide::proto::PasswordChangeResponse submit_form_response;
  logs_uploader.SetSubmitFormQuality(submit_form_response, CreateLoggingData(),
                                     fake_start_time);

  // Set verify submission data.
  optimization_guide::proto::PasswordChangeResponse verify_submission_response;
  logs_uploader.SetVerifySubmissionQuality(
      verify_submission_response, CreateLoggingData(), fake_start_time);

  // Verify that all steps have latency set.
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  EXPECT_EQ(final_log.password_change_submission()
                .quality()
                .open_form()
                .request_latency_ms(),
            expected_latency_ms);
  EXPECT_EQ(final_log.password_change_submission()
                .quality()
                .open_form()
                .request_latency_ms(),
            expected_latency_ms);
  EXPECT_EQ(final_log.password_change_submission()
                .quality()
                .open_form()
                .request_latency_ms(),
            expected_latency_ms);
}
