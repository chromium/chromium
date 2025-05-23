// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"

#include <memory>
#include <vector>

#include "base/test/test_future.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/test_variations_service.h"
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

void CheckCommonQualityLogFields(
    const optimization_guide::proto::LogAiDataRequest& log,
    const std::string& expected_domain,
    const std::string& expected_language,
    const std::string& expected_country) {
  EXPECT_EQ(log.password_change_submission().quality().domain(),
            expected_domain);
  EXPECT_EQ(log.password_change_submission().quality().language(),
            expected_language);
  EXPECT_EQ(log.password_change_submission().quality().location(),
            expected_country);
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
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        enabled_state_provider_(/*consent=*/true,
                                /*enabled=*/true) {}
  ~ModelQualityLogsUploaderTest() override = default;

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void SetLanguageForClient(const std::string& language) {
    ChromeTranslateClient::FromWebContents(web_contents())
        ->GetTranslateManager()
        ->GetLanguageState()
        ->SetSourceLanguage(language);
  }

  void SetCountryCode(const std::string& country) {
    // Set variation service.
    variations::TestVariationsService::RegisterPrefs(prefs_.registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &prefs_, &enabled_state_provider_,
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);
    variations_service_ = std::make_unique<variations::TestVariationsService>(
        &prefs_, metrics_state_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetVariationsService(
        variations_service_.get());

    // This pref directly overrides any country detection logic within the
    // variations service.
    prefs_.SetString(variations::prefs::kVariationsCountry, country);
  }

  TestingPrefServiceSimple prefs_;
  metrics::TestEnabledStateProvider enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
};

TEST_F(ModelQualityLogsUploaderTest, VerifySubmissionSucessLog) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents());
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
  ModelQualityLogsUploader logs_uploader(web_contents());
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
  ModelQualityLogsUploader logs_uploader(web_contents());
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
  ModelQualityLogsUploader logs_uploader(web_contents());
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
  ModelQualityLogsUploader logs_uploader(web_contents());
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
  ModelQualityLogsUploader logs_uploader(web_contents());
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
  ModelQualityLogsUploader logs_uploader(web_contents());
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
  ModelQualityLogsUploader logs_uploader(web_contents());
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

TEST_F(ModelQualityLogsUploaderTest, OpenFormTargetElementNotFound) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents());
  // Set initial open form data for ACTION_SUCCESS status.
  optimization_guide::proto::PasswordChangeResponse open_form_response;
  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response, CreateLoggingData(),
                                   fake_start_time);
  const optimization_guide::proto::LogAiDataRequest intial_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      intial_log,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  // Call function that overwrites the status to ELEMENT_NOT_FOUND status.
  logs_uploader.OpenFormTargetElementNotFound();
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      final_log,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_F(ModelQualityLogsUploaderTest, FormNotDetectedAfterOpening) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents());
  // Set initial open form data for ACTION_SUCCESS status.
  optimization_guide::proto::PasswordChangeResponse open_form_response;
  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response, CreateLoggingData(),
                                   fake_start_time);
  const optimization_guide::proto::LogAiDataRequest intial_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      intial_log,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  // Call function that overwrites the status to FORM_NOT_FOUND status.
  logs_uploader.FormNotDetectedAfterOpening();
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      final_log,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FORM_NOT_FOUND);
}

TEST_F(ModelQualityLogsUploaderTest, LogGeneralInformationSetOnCreation) {
  const GURL url("http://www.url.com");
  NavigateAndCommit(url);
  ChromeTranslateClient::CreateForWebContents(web_contents());
  const std::string expected_language = "pt-br";
  const std::string expected_country = "US";
  SetLanguageForClient(expected_language);
  SetCountryCode(expected_country);
  ModelQualityLogsUploader logs_uploader(web_contents());

  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckCommonQualityLogFields(final_log, "url.com", expected_language,
                              expected_country);
}

TEST_F(ModelQualityLogsUploaderTest, CompleteLogWithGeneralInformation) {
  const base::Time fake_start_time = base::Time::Now();
  const GURL url("http://www.url.com");
  NavigateAndCommit(url);
  ChromeTranslateClient::CreateForWebContents(web_contents());
  const std::string expected_language = "bd";
  const std::string expected_country = "PE";
  SetLanguageForClient(expected_language);
  SetCountryCode(expected_country);
  ModelQualityLogsUploader logs_uploader(web_contents());

  optimization_guide::proto::PasswordChangeResponse open_form_response;
  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response, CreateLoggingData(),
                                   fake_start_time);

  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      final_log,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  CheckCommonQualityLogFields(final_log, "url.com", expected_language,
                              expected_country);
}
