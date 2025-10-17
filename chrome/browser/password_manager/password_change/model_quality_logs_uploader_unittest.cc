// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"

#include <memory>
#include <vector>

#include "base/test/protobuf_matchers.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/password_manager/core/browser/password_manager.h"
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
using FlowStep = optimization_guide::proto::PasswordChangeRequest::FlowStep;
using LoginPasswordType =
    optimization_guide::proto::LoginAttemptOutcome_PasswordType;
using ::optimization_guide::TestModelQualityLogsUploaderService;

namespace {

using base::test::EqualsProto;

constexpr char kChangePasswordURL[] = "https://example.com/password/";

void VerifyLoginCheckStep(
    const optimization_guide::proto::LogAiDataRequest& log,
    const QualityStatus& expected_status,
    const int expected_retry_count,
    bool was_skipped) {
  EXPECT_EQ(
      log.password_change_submission().quality().logged_in_check().status(),
      expected_status);
  EXPECT_EQ(log.password_change_submission()
                .quality()
                .logged_in_check()
                .classification_overridden_by_user(),
            was_skipped);
  EXPECT_EQ(log.password_change_submission()
                .quality()
                .logged_in_check()
                .retry_count(),
            expected_retry_count);
}

void CheckOpenFormStatus(
    const optimization_guide::proto::LogAiDataRequest& log,
    const optimization_guide::proto::PasswordChangeRequest& expected_request,
    const optimization_guide::proto::PasswordChangeResponse& expected_response,
    const QualityStatus& expected_status) {
  EXPECT_EQ(log.password_change_submission().quality().open_form().status(),
            expected_status);
  EXPECT_THAT(log.password_change_submission().quality().open_form().request(),
              EqualsProto(expected_request));
  EXPECT_THAT(log.password_change_submission().quality().open_form().response(),
              EqualsProto(expected_response));
}

void CheckSubmitFormStatus(
    const optimization_guide::proto::LogAiDataRequest& log,
    const optimization_guide::proto::PasswordChangeRequest& expected_request,
    const optimization_guide::proto::PasswordChangeResponse& expected_response,
    const QualityStatus& expected_status) {
  EXPECT_EQ(log.password_change_submission().quality().submit_form().status(),
            expected_status);
  EXPECT_THAT(
      log.password_change_submission().quality().submit_form().request(),
      EqualsProto(expected_request));
  EXPECT_THAT(
      log.password_change_submission().quality().submit_form().response(),
      EqualsProto(expected_response));
}

void CheckVerifySubmissionStatus(
    const optimization_guide::proto::LogAiDataRequest& log,
    const optimization_guide::proto::PasswordChangeRequest& expected_request,
    const optimization_guide::proto::PasswordChangeResponse& expected_response,
    const QualityStatus& expected_status,
    const FinalModelStatus& expected_final_status) {
  EXPECT_EQ(log.password_change_submission().quality().final_model_status(),
            expected_final_status);
  EXPECT_EQ(
      log.password_change_submission().quality().verify_submission().status(),
      expected_status);
  EXPECT_THAT(
      log.password_change_submission().quality().verify_submission().request(),
      EqualsProto(expected_request));
  EXPECT_THAT(
      log.password_change_submission().quality().verify_submission().response(),
      EqualsProto(expected_response));
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
CreateLoggingData(
    const optimization_guide::proto::PasswordChangeRequest& request) {
  auto logging_data = std::make_unique<
      optimization_guide::proto::PasswordChangeSubmissionLoggingData>();
  logging_data->mutable_request()->CopyFrom(request);
  return logging_data;
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

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    mock_optimization_guide_keyed_service_ = static_cast<
        MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<MockOptimizationGuideKeyedService>();
                })));
    auto logs_uploader = std::make_unique<TestModelQualityLogsUploaderService>(
        TestingBrowserProcess::GetGlobal()->local_state());
    mock_optimization_guide_keyed_service_
        ->SetModelQualityLogsUploaderServiceForTesting(
            std::move(logs_uploader));

    // Set up requests for testing.
    open_form_request_.set_step(
        optimization_guide::proto::PasswordChangeRequest::FlowStep::
            PasswordChangeRequest_FlowStep_OPEN_FORM_STEP);
    *open_form_request_.mutable_page_context()->mutable_title() =
        "open_form_step";
    submit_form_request_.set_step(
        optimization_guide::proto::PasswordChangeRequest::FlowStep::
            PasswordChangeRequest_FlowStep_SUBMIT_FORM_STEP);
    *submit_form_request_.mutable_page_context()->mutable_title() =
        "submit_form_step";
    verify_submission_request_.set_step(
        optimization_guide::proto::PasswordChangeRequest::FlowStep::
            PasswordChangeRequest_FlowStep_VERIFY_SUBMISSION_STEP);
    *verify_submission_request_.mutable_page_context()->mutable_title() =
        "verify_submission_step";
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
    mock_optimization_guide_keyed_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void SetLanguageForClient(const std::string& language) {
    ChromeTranslateClient::FromWebContents(web_contents())
        ->GetTranslateManager()
        ->GetLanguageState()
        ->SetSourceLanguage(language);
  }

  void VerifyUniqueLoginAttemptLog(const std::string& expected_domain,
                                   LoginPasswordType expected_password_type,
                                   bool expected_success) {
    const std::vector<
        std::unique_ptr<optimization_guide::proto::LogAiDataRequest>>& logs =
        mqls_uploader_service()->uploaded_logs();
    ASSERT_EQ(1u, logs.size());
    optimization_guide::proto::LoginAttemptOutcome login_attempt_outcome =
        logs[0]->password_change_submission().login_attempt_outcome();
    EXPECT_EQ(login_attempt_outcome.domain(), expected_domain);
    EXPECT_EQ(login_attempt_outcome.success(), expected_success);
    EXPECT_EQ(login_attempt_outcome.password_type(), expected_password_type);
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

  TestModelQualityLogsUploaderService* mqls_uploader_service() {
    return static_cast<TestModelQualityLogsUploaderService*>(
        mock_optimization_guide_keyed_service_
            ->GetModelQualityLogsUploaderService());
  }

  optimization_guide::proto::PasswordChangeRequest open_form_request_;
  optimization_guide::proto::PasswordChangeRequest submit_form_request_;
  optimization_guide::proto::PasswordChangeRequest verify_submission_request_;

  TestingPrefServiceSimple prefs_;
  metrics::TestEnabledStateProvider enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_ = nullptr;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
};

TEST_F(ModelQualityLogsUploaderTest, VerifySubmissionSucessLog) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_outcome_data()->set_submission_outcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);
  logs_uploader.SetVerifySubmissionQuality(
      std::optional(response), CreateLoggingData(verify_submission_request_),
      fake_start_time);
  CheckVerifySubmissionStatus(
      logs_uploader.GetFinalLog(), verify_submission_request_, response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      FinalModelStatus::FINAL_MODEL_STATUS_SUCCESS);
}

TEST_F(ModelQualityLogsUploaderTest, OpenFormSuccessLog) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(std::optional(response),
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);

  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(), open_form_request_, response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
}

TEST_F(ModelQualityLogsUploaderTest, OpenFormElementNotFoundLog) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  logs_uploader.SetOpenFormQuality(std::optional(response),
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);
  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(), open_form_request_, response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_F(ModelQualityLogsUploaderTest, OpenFormUnexpectedStateLog) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_LOG_IN_PAGE);
  logs_uploader.SetOpenFormQuality(std::optional(response),
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);
  CheckOpenFormStatus(
      logs_uploader.GetFinalLog(), open_form_request_, response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE);
}

TEST_F(ModelQualityLogsUploaderTest, SubmitFormSuccessLog) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_submit_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetSubmitFormQuality(std::optional(response),
                                     CreateLoggingData(submit_form_request_),
                                     fake_start_time);
  CheckSubmitFormStatus(
      logs_uploader.GetFinalLog(), submit_form_request_, response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
}

TEST_F(ModelQualityLogsUploaderTest, SubmitFormElementNotFoundLog) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  optimization_guide::proto::PasswordChangeResponse response;
  logs_uploader.SetSubmitFormQuality(std::optional(response),
                                     CreateLoggingData(submit_form_request_),
                                     fake_start_time);
  CheckSubmitFormStatus(
      logs_uploader.GetFinalLog(), submit_form_request_, response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_F(ModelQualityLogsUploaderTest, MergeLogsDoesNotOverwrite) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  // Set open form data.
  optimization_guide::proto::PasswordChangeResponse open_form_response;
  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response,
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);

  // Set submit form data.
  optimization_guide::proto::PasswordChangeResponse submit_form_response;
  submit_form_response.mutable_submit_form_data()->set_dom_node_id_to_click(
      123);
  logs_uploader.SetSubmitFormQuality(submit_form_response,
                                     CreateLoggingData(submit_form_request_),
                                     fake_start_time);

  // Set verify submission data.
  optimization_guide::proto::PasswordChangeResponse verify_submission_response;
  logs_uploader.SetVerifySubmissionQuality(
      verify_submission_response, CreateLoggingData(verify_submission_request_),
      fake_start_time);

  // Verify all steps have quality data and it is not overwritten.
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();

  CheckOpenFormStatus(
      final_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  CheckSubmitFormStatus(
      final_log, submit_form_request_, submit_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  CheckVerifySubmissionStatus(
      final_log, verify_submission_request_, verify_submission_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      FinalModelStatus::FINAL_MODEL_STATUS_SUCCESS);
}

TEST_F(ModelQualityLogsUploaderTest, LatencyRecordedForAllSteps) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  constexpr int64_t expected_latency_ms = 2;
  constexpr base::TimeDelta latency = base::Milliseconds(expected_latency_ms);

  task_environment()->FastForwardBy(latency);
  // Set open form data.
  optimization_guide::proto::PasswordChangeResponse open_form_response;

  logs_uploader.SetOpenFormQuality(open_form_response,
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);

  // Set submit form data.
  optimization_guide::proto::PasswordChangeResponse submit_form_response;
  logs_uploader.SetSubmitFormQuality(submit_form_response,
                                     CreateLoggingData(submit_form_request_),
                                     fake_start_time);

  // Set verify submission data.
  optimization_guide::proto::PasswordChangeResponse verify_submission_response;
  logs_uploader.SetVerifySubmissionQuality(
      verify_submission_response, CreateLoggingData(verify_submission_request_),
      fake_start_time);

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
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  // Set initial open form data for ACTION_SUCCESS status.
  optimization_guide::proto::PasswordChangeResponse open_form_response;

  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response,
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);
  const optimization_guide::proto::LogAiDataRequest initial_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      initial_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  // Call function that overwrites the status to ELEMENT_NOT_FOUND status.
  logs_uploader.OpenFormTargetElementNotFound();
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      final_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_F(ModelQualityLogsUploaderTest, LoginCheckSkipped) {
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  logs_uploader.LoginCheckSkipped();
  VerifyLoginCheckStep(
      logs_uploader.GetFinalLog(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*expected_retry_count=*/0,
      /*was_skipped=*/true);
}

TEST_F(ModelQualityLogsUploaderTest, LoginCheckRetryCountSet) {
  const int login_state_checks = 3;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  QualityStatus quality_status = QualityStatus::
      PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS;
  logs_uploader.SetLoggedInCheckQuality(login_state_checks, quality_status);
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  VerifyLoginCheckStep(logs_uploader.GetFinalLog(), quality_status,
                       /*expected_retry_count=*/login_state_checks - 1,
                       /*was_skipped=*/false);
}

TEST_F(ModelQualityLogsUploaderTest, LoginCheckReachedMaxAttempts) {
  const int login_state_checks = 5;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  QualityStatus quality_status = QualityStatus::
      PasswordChangeQuality_StepQuality_SubmissionStatus_FAILURE_STATUS;
  logs_uploader.SetLoggedInCheckQuality(login_state_checks, quality_status);
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  VerifyLoginCheckStep(logs_uploader.GetFinalLog(), quality_status,
                       /*expected_retry_count=*/login_state_checks - 1,
                       /*was_skipped=*/false);
}

TEST_F(ModelQualityLogsUploaderTest, LastLoginCheckHadUnexpectedState) {
  const int login_state_checks = 5;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  QualityStatus unexpected_status = QualityStatus::
      PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE;
  logs_uploader.SetLoggedInCheckQuality(login_state_checks, unexpected_status);
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  VerifyLoginCheckStep(logs_uploader.GetFinalLog(), unexpected_status,
                       /*expected_retry_count=*/login_state_checks - 1,
                       /*was_skipped=*/false);
}

TEST_F(ModelQualityLogsUploaderTest, FlowInterruptedAfterLoginCheck) {
  const int login_state_checks = 3;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  const QualityStatus success_status = QualityStatus::
      PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS;
  logs_uploader.SetLoggedInCheckQuality(login_state_checks, success_status);
  logs_uploader.SetFlowInterrupted();
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  VerifyLoginCheckStep(final_log, success_status,
                       /*expected_retry_count=*/login_state_checks - 1,
                       /*was_skipped=*/false);
  CheckOpenFormStatus(
      final_log,
      /*expected_request=*/optimization_guide::proto::PasswordChangeRequest(),
      /*expected_response=*/optimization_guide::proto::PasswordChangeResponse(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED);
}

TEST_F(ModelQualityLogsUploaderTest, LoginCheckStepOtpDetected) {
  const int login_state_checks = 3;
  ModelQualityLogsUploader logs_uploader(web_contents(), GURL());
  QualityStatus quality_status = QualityStatus::
      PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS;
  logs_uploader.SetLoggedInCheckQuality(login_state_checks, quality_status);
  logs_uploader.SetOtpDetected();
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  VerifyLoginCheckStep(final_log, quality_status,
                       /*expected_retry_count=*/login_state_checks - 1,
                       /*was_skipped=*/false);
  CheckOpenFormStatus(
      final_log,
      /*expected_request=*/optimization_guide::proto::PasswordChangeRequest(),
      /*expected_response=*/optimization_guide::proto::PasswordChangeResponse(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_OTP_DETECTED);
}

TEST_F(ModelQualityLogsUploaderTest, OpenFormFlowInterrupted) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  // Set initial open form data for ACTION_SUCCESS status.
  optimization_guide::proto::PasswordChangeResponse open_form_response;

  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response,
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);
  const optimization_guide::proto::LogAiDataRequest initial_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      initial_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  logs_uploader.SetFlowInterrupted();
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      final_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  CheckSubmitFormStatus(
      final_log,
      /*expected_request=*/optimization_guide::proto::PasswordChangeRequest(),
      /*expected_response=*/optimization_guide::proto::PasswordChangeResponse(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED);
}

TEST_F(ModelQualityLogsUploaderTest, SubmitFormFlowInterrupted) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  // Set open form data.
  optimization_guide::proto::PasswordChangeResponse open_form_response;

  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response,
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);

  // Set submit form data.
  optimization_guide::proto::PasswordChangeResponse submit_form_response;
  submit_form_response.mutable_submit_form_data()->set_dom_node_id_to_click(
      123);
  logs_uploader.SetSubmitFormQuality(submit_form_response,
                                     CreateLoggingData(submit_form_request_),
                                     fake_start_time);

  // This should override the most recent log, which is for SUBMIT_FORM.
  logs_uploader.SetFlowInterrupted();
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();

  CheckOpenFormStatus(
      final_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  CheckSubmitFormStatus(
      final_log, submit_form_request_, submit_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  CheckVerifySubmissionStatus(
      logs_uploader.GetFinalLog(),
      /*expected_request=*/optimization_guide::proto::PasswordChangeRequest(),
      /*expected_response=*/optimization_guide::proto::PasswordChangeResponse(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED,
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

TEST_F(ModelQualityLogsUploaderTest, OpenFormOtpDetected) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  // Set initial open form data for ACTION_SUCCESS status.
  optimization_guide::proto::PasswordChangeResponse open_form_response;

  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response,
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);
  const optimization_guide::proto::LogAiDataRequest initial_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      initial_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  logs_uploader.SetOtpDetected();
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      final_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  CheckSubmitFormStatus(
      final_log,
      /*expected_request=*/optimization_guide::proto::PasswordChangeRequest(),
      /*expected_response=*/optimization_guide::proto::PasswordChangeResponse(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_OTP_DETECTED);
}

TEST_F(ModelQualityLogsUploaderTest, SubmitFormOtpDetected) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  // Set open form data.
  optimization_guide::proto::PasswordChangeResponse open_form_response;

  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response,
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);

  // Set submit form data.
  optimization_guide::proto::PasswordChangeResponse submit_form_response;
  submit_form_response.mutable_submit_form_data()->set_dom_node_id_to_click(
      123);
  logs_uploader.SetSubmitFormQuality(submit_form_response,
                                     CreateLoggingData(submit_form_request_),
                                     fake_start_time);

  // This should override the most recent log, which is for SUBMIT_FORM.
  logs_uploader.SetOtpDetected();
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();

  CheckOpenFormStatus(
      final_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  CheckSubmitFormStatus(
      final_log, submit_form_request_, submit_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  CheckVerifySubmissionStatus(
      logs_uploader.GetFinalLog(),
      /*expected_request=*/optimization_guide::proto::PasswordChangeRequest(),
      /*expected_response=*/optimization_guide::proto::PasswordChangeResponse(),
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_OTP_DETECTED,
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

TEST_F(ModelQualityLogsUploaderTest, OpenFormSkipped) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  // Set initial open form data for ACTION_SUCCESS status.
  optimization_guide::proto::PasswordChangeResponse open_form_response;

  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response,
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);
  const optimization_guide::proto::LogAiDataRequest initial_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      initial_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  logs_uploader.MarkStepSkipped(
      FlowStep::PasswordChangeRequest_FlowStep_OPEN_FORM_STEP);
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      final_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED);
}

TEST_F(ModelQualityLogsUploaderTest, SubmitFormSkipped) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  // Set initial submit form data for ACTION_SUCCESS status.
  optimization_guide::proto::PasswordChangeResponse submit_form_response;
  submit_form_response.mutable_submit_form_data()->set_dom_node_id_to_click(
      123);
  logs_uploader.SetSubmitFormQuality(submit_form_response,
                                     CreateLoggingData(submit_form_request_),
                                     fake_start_time);
  const optimization_guide::proto::LogAiDataRequest initial_log =
      logs_uploader.GetFinalLog();
  CheckSubmitFormStatus(
      initial_log, submit_form_request_, submit_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  logs_uploader.MarkStepSkipped(
      FlowStep::PasswordChangeRequest_FlowStep_SUBMIT_FORM_STEP);
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckSubmitFormStatus(
      final_log, submit_form_request_, submit_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED);
}

TEST_F(ModelQualityLogsUploaderTest, SubmitFormTargetElementNotFound) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  // Set initial submit form data for ACTION_SUCCESS status.
  optimization_guide::proto::PasswordChangeResponse submit_form_response;
  submit_form_response.mutable_submit_form_data()->set_dom_node_id_to_click(-5);
  logs_uploader.SetSubmitFormQuality(submit_form_response,
                                     CreateLoggingData(submit_form_request_),
                                     fake_start_time);
  const optimization_guide::proto::LogAiDataRequest initial_log =
      logs_uploader.GetFinalLog();
  CheckSubmitFormStatus(
      initial_log, submit_form_request_, submit_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  // Call function that overwrites the status to ELEMENT_NOT_FOUND status.
  logs_uploader.SubmitFormTargetElementNotFound();
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckSubmitFormStatus(
      final_log, submit_form_request_, submit_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

TEST_F(ModelQualityLogsUploaderTest, FormNotDetectedAfterOpening) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  // Set initial open form data for ACTION_SUCCESS status.
  optimization_guide::proto::PasswordChangeResponse open_form_response;

  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response,
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);
  const optimization_guide::proto::LogAiDataRequest initial_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      initial_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  // Call function that overwrites the status to FORM_NOT_FOUND status.
  logs_uploader.FormNotDetectedAfterOpening();
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      final_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FORM_NOT_FOUND);
}

TEST_F(ModelQualityLogsUploaderTest, OpenFormUnexpectedFailure) {
  const base::Time fake_start_time = base::Time::Now();
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));
  // Set initial open form data for ACTION_SUCCESS status.
  optimization_guide::proto::PasswordChangeResponse open_form_response;

  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response,
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);
  const optimization_guide::proto::LogAiDataRequest initial_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      initial_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  // Call function that overwrites the status to UNEXPECTED_STATE status.
  logs_uploader.SetOpenFormUnexpectedFailure();
  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      final_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE);
}

TEST_F(ModelQualityLogsUploaderTest, LogGeneralInformationSetOnCreation) {
  ChromeTranslateClient::CreateForWebContents(web_contents());
  const std::string expected_language = "pt-br";
  const std::string expected_country = "US";
  SetLanguageForClient(expected_language);
  SetCountryCode(expected_country);
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));

  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckCommonQualityLogFields(final_log, "example.com", expected_language,
                              expected_country);
}

TEST_F(ModelQualityLogsUploaderTest, CompleteLogWithGeneralInformation) {
  const base::Time fake_start_time = base::Time::Now();
  ChromeTranslateClient::CreateForWebContents(web_contents());
  const std::string expected_language = "bd";
  const std::string expected_country = "PE";
  SetLanguageForClient(expected_language);
  SetCountryCode(expected_country);
  ModelQualityLogsUploader logs_uploader(web_contents(),
                                         GURL(kChangePasswordURL));

  optimization_guide::proto::PasswordChangeResponse open_form_response;

  open_form_response.mutable_open_form_data()->set_page_type(
      PageType::OpenFormResponseData_PageType_SETTINGS_PAGE);
  open_form_response.mutable_open_form_data()->set_dom_node_id_to_click(123);
  logs_uploader.SetOpenFormQuality(open_form_response,
                                   CreateLoggingData(open_form_request_),
                                   fake_start_time);

  const optimization_guide::proto::LogAiDataRequest final_log =
      logs_uploader.GetFinalLog();
  CheckOpenFormStatus(
      final_log, open_form_request_, open_form_response,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  CheckCommonQualityLogFields(final_log, "example.com", expected_language,
                              expected_country);
}

TEST_F(ModelQualityLogsUploaderTest, RecordLogPrimaryPassword) {
  const GURL url("http://www.url.com");
  NavigateAndCommit(url);
  ModelQualityLogsUploader::RecordLoginAttemptQuality(
      mqls_uploader_service(), url,
      password_manager::LogInWithChangedPasswordOutcome::
          kPrimaryPasswordSucceeded);
  VerifyUniqueLoginAttemptLog(
      "url.com", LoginPasswordType::LoginAttemptOutcome_PasswordType_PRIMARY,
      true);
}

TEST_F(ModelQualityLogsUploaderTest, RecordLogBackupPassword) {
  const GURL url("http://www.url.com");
  NavigateAndCommit(url);
  ModelQualityLogsUploader::RecordLoginAttemptQuality(
      mqls_uploader_service(), url,
      password_manager::LogInWithChangedPasswordOutcome::kBackupPasswordFailed);
  VerifyUniqueLoginAttemptLog(
      "url.com", LoginPasswordType::LoginAttemptOutcome_PasswordType_BACKUP,
      false);
}

TEST_F(ModelQualityLogsUploaderTest, RecordLogUnknownPassword) {
  const GURL url("http://www.url.com");
  NavigateAndCommit(url);
  ModelQualityLogsUploader::RecordLoginAttemptQuality(
      mqls_uploader_service(), url,
      password_manager::LogInWithChangedPasswordOutcome::
          kUnknownPasswordFailed);
  VerifyUniqueLoginAttemptLog(
      "url.com", LoginPasswordType::LoginAttemptOutcome_PasswordType_UNKNOWN,
      false);
}
