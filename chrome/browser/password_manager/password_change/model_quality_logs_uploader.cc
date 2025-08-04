// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

using FinalModelStatus = optimization_guide::proto::FinalModelStatus;
using PasswordChangeOutcome = optimization_guide::proto ::
    PasswordChangeSubmissionData_PasswordChangeOutcome;
using PageType = optimization_guide::proto::OpenFormResponseData_PageType;
using LoginPasswordType =
    optimization_guide::proto::LoginAttemptOutcome_PasswordType;

namespace {
int64_t ComputeRequestLatencyMs(base::Time server_request_start_time) {
  return (base::Time::Now() - server_request_start_time).InMilliseconds();
}

std::string GetLocation() {
  variations::VariationsService* variation_service =
      g_browser_process->variations_service();
  return variation_service
             ? base::ToUpperASCII(variation_service->GetLatestCountry())
             : std::string();
}

std::string GetPageDomain(const GURL& page_url) {
  return affiliations::GetExtendedTopLevelDomain(page_url,
                                                 /*psl_extensions=*/{});
}

std::string GetPageLanguage(content::WebContents* web_contents) {
  CHECK(web_contents);
  auto* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(web_contents);
  if (translate_manager) {
    return translate_manager->GetLanguageState()->source_language();
  }
  return std::string();
}

FinalModelStatus GetFinalModelStatus(
    const std::optional<optimization_guide::proto::PasswordChangeResponse>&
        response) {
  if (!response.has_value()) {
    return FinalModelStatus::FINAL_MODEL_STATUS_FAILURE;
  }
  PasswordChangeOutcome outcome =
      response.value().outcome_data().submission_outcome();
  if (outcome !=
          PasswordChangeOutcome::
              PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME &&
      outcome !=
          PasswordChangeOutcome::
              PasswordChangeSubmissionData_PasswordChangeOutcome_UNKNOWN_OUTCOME) {
    return FinalModelStatus::FINAL_MODEL_STATUS_FAILURE;
  }
  return FinalModelStatus::FINAL_MODEL_STATUS_SUCCESS;
}

ModelQualityLogsUploader::QualityStatus GetVerifySubmissionQualityStatus(
    const std::optional<optimization_guide::proto::PasswordChangeResponse>&
        response) {
  if (!response.has_value()) {
    return ModelQualityLogsUploader::QualityStatus::
        PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE;
  }

  PasswordChangeOutcome outcome =
      response.value().outcome_data().submission_outcome();
  if (outcome !=
          PasswordChangeOutcome::
              PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME &&
      outcome !=
          PasswordChangeOutcome::
              PasswordChangeSubmissionData_PasswordChangeOutcome_UNKNOWN_OUTCOME) {
    return ModelQualityLogsUploader::QualityStatus::
        PasswordChangeQuality_StepQuality_SubmissionStatus_FAILURE_STATUS;
  }
  return ModelQualityLogsUploader::QualityStatus::
      PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS;
}

optimization_guide::proto::PasswordChangeQuality_StepQuality*
GetNextStepQuality(optimization_guide::proto::LogAiDataRequest& log) {
  optimization_guide::proto::PasswordChangeQuality* quality =
      log.mutable_password_change_submission()->mutable_quality();
  if (quality->submit_form().status() !=
      ModelQualityLogsUploader::QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS) {
    return quality->mutable_verify_submission();
  }

  if (quality->open_form().status() !=
      ModelQualityLogsUploader::QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS) {
    return quality->mutable_submit_form();
  }
  return quality->mutable_open_form();
}

optimization_guide::proto::PasswordChangeQuality_StepQuality* GetStepQuality(
    ModelQualityLogsUploader::FlowStep step,
    optimization_guide::proto::LogAiDataRequest& log) {
  optimization_guide::proto::PasswordChangeQuality* quality =
      log.mutable_password_change_submission()->mutable_quality();
  switch (step) {
    case ModelQualityLogsUploader::FlowStep::
        PasswordChangeRequest_FlowStep_OPEN_FORM_STEP:
      return quality->mutable_open_form();
    case ModelQualityLogsUploader::FlowStep::
        PasswordChangeRequest_FlowStep_SUBMIT_FORM_STEP:
      return quality->mutable_submit_form();
    case ModelQualityLogsUploader::FlowStep::
        PasswordChangeRequest_FlowStep_VERIFY_SUBMISSION_STEP:
      return quality->mutable_verify_submission();
    default:
      NOTREACHED();
  }
}

bool IsSuccessfulLoginAttempt(
    password_manager::LogInWithChangedPasswordOutcome login_outcome) {
  switch (login_outcome) {
    case password_manager::LogInWithChangedPasswordOutcome::
        kBackupPasswordSucceeded:
    case password_manager::LogInWithChangedPasswordOutcome::
        kPrimaryPasswordSucceeded:
      // TODO(crbug.com/425927757): Add Unknown case.
      return true;
    default:
      return false;
  }
}

LoginPasswordType GetLoginAttemptPasswordType(
    password_manager::LogInWithChangedPasswordOutcome login_outcome) {
  switch (login_outcome) {
    case password_manager::LogInWithChangedPasswordOutcome::
        kPrimaryPasswordSucceeded:
    case password_manager::LogInWithChangedPasswordOutcome::
        kPrimaryPasswordFailed:
      return LoginPasswordType::LoginAttemptOutcome_PasswordType_PRIMARY;
    case password_manager::LogInWithChangedPasswordOutcome::
        kBackupPasswordFailed:
    case password_manager::LogInWithChangedPasswordOutcome::
        kBackupPasswordSucceeded:
      return LoginPasswordType::LoginAttemptOutcome_PasswordType_BACKUP;
    default:
      return LoginPasswordType::LoginAttemptOutcome_PasswordType_UNKNOWN;
  }
}

}  // namespace

ModelQualityLogsUploader::ModelQualityLogsUploader(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  SetCommonInformationQuality(web_contents);
}
ModelQualityLogsUploader::~ModelQualityLogsUploader() = default;

void ModelQualityLogsUploader::SetCommonInformationQuality(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  const GURL& page_url =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->set_domain(GetPageDomain(page_url));
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->set_location(GetLocation());
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->set_language(GetPageLanguage(web_contents));
}

void ModelQualityLogsUploader::SetOpenFormQuality(
    const std::optional<optimization_guide::proto::PasswordChangeResponse>&
        response,
    std::unique_ptr<LoggingData> logging_data,
    base::Time server_request_start_time) {
  if (!logging_data) {
    return;
  }

  QualityStatus quality_status = QualityStatus::
      PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS;
  if (response.has_value()) {
    PageType open_form = response.value().open_form_data().page_type();
    if (open_form == PageType::OpenFormResponseData_PageType_SETTINGS_PAGE) {
      if (response.value().open_form_data().dom_node_id_to_click()) {
        // Assume success at this point. If it fails to actuate on it the state
        // will be changed to ELEMENT_NOT_FOUND if the element does not exist
        // or FORM_NOT_FOUND if after clicking a form was not seen.
        quality_status = QualityStatus::
            PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS;
      } else {
        quality_status = QualityStatus::
            PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND;
      }
    } else {
      quality_status = QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE;
    }
  }

  final_log_data_.mutable_password_change_submission()->MergeFrom(
      *logging_data);
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->mutable_open_form()
      ->set_status(quality_status);
  // Set latency
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->mutable_open_form()
      ->set_request_latency_ms(
          ComputeRequestLatencyMs(server_request_start_time));
}

void ModelQualityLogsUploader::FormNotDetectedAfterOpening() {
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->mutable_open_form()
      ->set_status(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_FORM_NOT_FOUND);
}

void ModelQualityLogsUploader::SetOpenFormUnexpectedFailure() {
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->mutable_open_form()
      ->set_status(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE);
}

void ModelQualityLogsUploader::SetFlowInterrupted() {
  GetNextStepQuality(final_log_data_)
      ->set_status(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED);
}

void ModelQualityLogsUploader::SetOtpDetected() {
  GetNextStepQuality(final_log_data_)
      ->set_status(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_OTP_DETECTED);
}

void ModelQualityLogsUploader::MarkStepSkipped(
    optimization_guide::proto::PasswordChangeRequest::FlowStep step) {
  GetStepQuality(step, final_log_data_)
      ->set_status(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED);
}

void ModelQualityLogsUploader::OpenFormTargetElementNotFound() {
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->mutable_open_form()
      ->set_status(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

void ModelQualityLogsUploader::SubmitFormTargetElementNotFound() {
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->mutable_submit_form()
      ->set_status(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

void ModelQualityLogsUploader::SetSubmitFormQuality(
    const std::optional<optimization_guide::proto::PasswordChangeResponse>&
        response,
    std::unique_ptr<LoggingData> logging_data,
    base::Time server_request_start_time) {
  if (!logging_data) {
    return;
  }
  QualityStatus quality_status = QualityStatus::
      PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS;
  if (response.has_value()) {
    if (response.value().submit_form_data().dom_node_id_to_click()) {
      quality_status = QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS;
    } else {
      quality_status = QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND;
    }
  }

  final_log_data_.mutable_password_change_submission()->MergeFrom(
      *logging_data);
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->mutable_submit_form()
      ->set_status(quality_status);
  // Set latency
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->mutable_submit_form()
      ->set_request_latency_ms(
          ComputeRequestLatencyMs(server_request_start_time));
}

void ModelQualityLogsUploader::SetVerifySubmissionQuality(
    const std::optional<optimization_guide::proto::PasswordChangeResponse>&
        response,
    std::unique_ptr<LoggingData> logging_data,
    base::Time server_request_start_time) {
  if (!logging_data) {
    return;
  }
  FinalModelStatus final_model_status = GetFinalModelStatus(response);
  QualityStatus quality_status = GetVerifySubmissionQualityStatus(response);

  final_log_data_.mutable_password_change_submission()->MergeFrom(
      *logging_data);
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->mutable_verify_submission()
      ->set_status(quality_status);
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->set_final_model_status(final_model_status);
  // Set latency
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->mutable_verify_submission()
      ->set_request_latency_ms(
          ComputeRequestLatencyMs(server_request_start_time));
}

// static
void ModelQualityLogsUploader::RecordLoginAttemptQuality(
    optimization_guide::ModelQualityLogsUploaderService* mqls_service,
    const GURL& page_url,
    password_manager::LogInWithChangedPasswordOutcome login_outcome) {
  CHECK(mqls_service);
  auto new_log_entry =
      std::make_unique<optimization_guide::ModelQualityLogEntry>(
          mqls_service->GetWeakPtr());
  auto* login_attempt_outcome = new_log_entry->log_ai_data_request()
                                    ->mutable_password_change_submission()
                                    ->mutable_login_attempt_outcome();
  login_attempt_outcome->set_domain(GetPageDomain(page_url));
  login_attempt_outcome->set_success(IsSuccessfulLoginAttempt(login_outcome));
  login_attempt_outcome->set_password_type(
      GetLoginAttemptPasswordType(login_outcome));
}

void ModelQualityLogsUploader::UploadFinalLog() {
  auto* mqls_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_)
          ->GetModelQualityLogsUploaderService();
  if (!mqls_service) {
    return;
  }
  auto new_log_entry =
      std::make_unique<optimization_guide::ModelQualityLogEntry>(
          mqls_service->GetWeakPtr());

  new_log_entry->log_ai_data_request()->MergeFrom(final_log_data_);
  optimization_guide::ModelQualityLogEntry::Upload(std::move(new_log_entry));
}
