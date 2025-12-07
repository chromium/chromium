// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change/login_state_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/password_manager/core/browser/password_form.h"
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
using FormData = optimization_guide::proto::PasswordChangeQuality_FormData;
using FieldData =
    optimization_guide::proto::PasswordChangeQuality_FormData_FieldData;
using FieldType = optimization_guide::proto::
    PasswordChangeQuality_FormData_FieldData_FieldType;

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

optimization_guide::proto::PasswordChangeQuality_StepQuality* GetStepQuality(
    ModelQualityLogsUploader::FlowStep step,
    optimization_guide::proto::LogAiDataRequest& log) {
  optimization_guide::proto::PasswordChangeQuality* quality =
      log.mutable_password_change_submission()->mutable_quality();
  switch (step) {
    case ModelQualityLogsUploader::FlowStep::
        PasswordChangeRequest_FlowStep_IS_LOGGED_IN_STEP:
      return quality->mutable_logged_in_check();
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
    case password_manager::LogInWithChangedPasswordOutcome::
        kUnknownPasswordSucceeded:
      return true;
    case password_manager::LogInWithChangedPasswordOutcome::
        kBackupPasswordFailed:
    case password_manager::LogInWithChangedPasswordOutcome::
        kPrimaryPasswordFailed:
    case password_manager::LogInWithChangedPasswordOutcome::
        kUnknownPasswordFailed:
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
    case password_manager::LogInWithChangedPasswordOutcome::
        kUnknownPasswordFailed:
    case password_manager::LogInWithChangedPasswordOutcome::
        kUnknownPasswordSucceeded:
      return LoginPasswordType::LoginAttemptOutcome_PasswordType_UNKNOWN;
  }
}

bool ReachedAttemptsLimit(int state_checks_count) {
  return state_checks_count >= LoginStateChecker::kMaxLoginChecks;
}

ModelQualityLogsUploader::QualityStatus GetStepStatus(
    actor::mojom::ActionResultCode failure) {
  CHECK_NE(actor::mojom::ActionResultCode::kOk, failure);
  switch (failure) {
    case actor::mojom::ActionResultCode::kInvalidDomNodeId:
      return ModelQualityLogsUploader::QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND;
    case actor::mojom::ActionResultCode::kElementDisabled:
      return ModelQualityLogsUploader::QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_DISABLED;
    case actor::mojom::ActionResultCode::kElementOffscreen:
      return ModelQualityLogsUploader::QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_OFFSCREEN;
    case actor::mojom::ActionResultCode::kTargetNodeInteractionPointObscured:
      return ModelQualityLogsUploader::QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_OBSCURED;
    default:
      return ModelQualityLogsUploader::QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE;
  }
}

FieldType GetFieldType(const autofill::FormFieldData& field,
                       const password_manager::PasswordForm& form) {
  autofill::FieldRendererId field_id = field.renderer_id();
  if (field_id == form.username_element_renderer_id) {
    return optimization_guide::proto::
        PasswordChangeQuality_FormData_FieldData_FieldType_USERNAME;
  } else if (field_id == form.password_element_renderer_id) {
    return optimization_guide::proto::
        PasswordChangeQuality_FormData_FieldData_FieldType_PASSWORD;
  } else if (field_id == form.new_password_element_renderer_id) {
    return optimization_guide::proto::
        PasswordChangeQuality_FormData_FieldData_FieldType_NEW_PASSWORD;
  } else if (field_id == form.confirmation_password_element_renderer_id) {
    return optimization_guide::proto::
        PasswordChangeQuality_FormData_FieldData_FieldType_CONFIRMATION_PASSWORD;
  }
  return optimization_guide::proto::
      PasswordChangeQuality_FormData_FieldData_FieldType_UNKNOWN;
}

void SetFormData(FormData& form_data_proto,
                 const password_manager::PasswordForm& form) {
  form_data_proto.set_form_signature(
      autofill::CalculateFormSignature(form.form_data).value());
  form_data_proto.set_form_id(base::UTF16ToUTF8(form.form_data.id_attribute()));
  form_data_proto.set_form_name(
      base::UTF16ToUTF8(form.form_data.name_attribute()));
  form_data_proto.set_url(form.url.spec());
  for (const auto& button : form.form_data.button_titles()) {
    form_data_proto.add_button_text(base::UTF16ToUTF8(button.first));
  }
  for (const auto& field : form.form_data.fields()) {
    FieldData field_data;
    field_data.set_signature(
        autofill::CalculateFieldSignatureForField(field).value());
    field_data.set_id(base::UTF16ToUTF8(field.id_attribute()));
    field_data.set_name(base::UTF16ToUTF8(field.name_attribute()));
    field_data.set_label(base::UTF16ToUTF8(field.label()));
    field_data.set_html_type(
        autofill::FormControlTypeToString(field.form_control_type()));
    field_data.set_placeholder(base::UTF16ToUTF8(field.placeholder()));
    field_data.set_field_type(GetFieldType(field, form));
    *form_data_proto.add_field_data() = field_data;
  }
}

}  // namespace

ModelQualityLogsUploader::ModelQualityLogsUploader(
    content::WebContents* web_contents,
    const GURL& change_password_url)
    : flow_start_time_(base::Time::Now()) {
  CHECK(web_contents);
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());

  auto* quality =
      final_log_data_.mutable_password_change_submission()->mutable_quality();
  quality->set_domain(GetPageDomain(change_password_url));
  quality->set_location(GetLocation());
  quality->set_language(GetPageLanguage(web_contents));
}
ModelQualityLogsUploader::~ModelQualityLogsUploader() = default;

void ModelQualityLogsUploader::SetLoggedInCheckQuality(
    int state_checks_count,
    std::unique_ptr<LoggingData> logging_data) {
  if (!logging_data) {
    return;
  }

  optimization_guide::proto::PasswordChangeQuality_StepQuality*
      logged_in_check_quality =
          final_log_data_.mutable_password_change_submission()
              ->mutable_quality()
              ->mutable_logged_in_check();

  logged_in_check_quality->mutable_request()->CopyFrom(logging_data->request());
  logged_in_check_quality->mutable_response()->CopyFrom(
      logging_data->response());

  QualityStatus quality_status;
  if (logging_data->response().is_logged_in_data().is_logged_in()) {
    quality_status = QualityStatus::
        PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS;
  } else if (ReachedAttemptsLimit(state_checks_count)) {
    quality_status = QualityStatus::
        PasswordChangeQuality_StepQuality_SubmissionStatus_FAILURE_STATUS;
  } else {
    quality_status = QualityStatus::
        PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE;
  }
  logged_in_check_quality->set_status(quality_status);
  // If the initial login check wasn't performed because the page content failed
  // to be requested, the state_checks_count can be 0.
  const int retry_count = std::max(0, state_checks_count - 1);
  logged_in_check_quality->set_retry_count(retry_count);
}

void ModelQualityLogsUploader::SetOpenFormQuality(
    const std::optional<optimization_guide::proto::PasswordChangeResponse>&
        response,
    std::unique_ptr<LoggingData> logging_data) {
  if (!logging_data) {
    return;
  }

  optimization_guide::proto::PasswordChangeQuality_StepQuality*
      open_form_quality = final_log_data_.mutable_password_change_submission()
                              ->mutable_quality()
                              ->mutable_open_form();

  QualityStatus quality_status = QualityStatus::
      PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS;

  if (response.has_value()) {
    open_form_quality->mutable_response()->CopyFrom(*response);
    PageType open_form = response->open_form_data().page_type();
    if (open_form == PageType::OpenFormResponseData_PageType_SETTINGS_PAGE) {
      if (response->open_form_data().dom_node_id_to_click()) {
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

  open_form_quality->mutable_request()->CopyFrom(logging_data->request());
  open_form_quality->set_status(quality_status);
}

void ModelQualityLogsUploader::SetSubmitFormQuality(
    const std::optional<optimization_guide::proto::PasswordChangeResponse>&
        response,
    std::unique_ptr<LoggingData> logging_data) {
  if (!logging_data) {
    return;
  }
  optimization_guide::proto::PasswordChangeQuality_StepQuality*
      submit_form_quality =
          final_log_data_.mutable_password_change_submission()
              ->mutable_quality()
              ->mutable_submit_form();

  QualityStatus quality_status = QualityStatus::
      PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS;
  if (response.has_value()) {
    submit_form_quality->mutable_response()->CopyFrom(*response);
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

  submit_form_quality->mutable_request()->CopyFrom(logging_data->request());
  submit_form_quality->set_status(quality_status);
}

void ModelQualityLogsUploader::SetVerifySubmissionQuality(
    const std::optional<optimization_guide::proto::PasswordChangeResponse>&
        response,
    std::unique_ptr<LoggingData> logging_data) {
  if (!logging_data) {
    return;
  }
  optimization_guide::proto::PasswordChangeQuality_StepQuality*
      verify_submission_quality =
          final_log_data_.mutable_password_change_submission()
              ->mutable_quality()
              ->mutable_verify_submission();

  QualityStatus quality_status = GetVerifySubmissionQualityStatus(response);
  final_log_data_.mutable_password_change_submission()->MergeFrom(
      *logging_data);

  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->set_final_model_status(GetFinalModelStatus(response));

  if (response.has_value()) {
    verify_submission_quality->mutable_response()->CopyFrom(*response);
  }
  verify_submission_quality->mutable_request()->CopyFrom(
      logging_data->request());
  verify_submission_quality->set_status(quality_status);
}

void ModelQualityLogsUploader::FormNotDetectedAfterOpening() {
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->mutable_open_form()
      ->set_status(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_FORM_NOT_FOUND);
}

void ModelQualityLogsUploader::SetFlowInterrupted(
    FlowStep step,
    QualityStatus quality_status) {
  GetStepQuality(step, final_log_data_)->set_status(quality_status);
}

void ModelQualityLogsUploader::MarkStepSkipped(
    optimization_guide::proto::PasswordChangeRequest::FlowStep step) {
  GetStepQuality(step, final_log_data_)
      ->set_status(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED);
}

void ModelQualityLogsUploader::RecordButtonClickFailure(
    FlowStep step,
    actor::mojom::ActionResultCode failure) {
  CHECK_NE(FlowStep::PasswordChangeRequest_FlowStep_IS_LOGGED_IN_STEP, step);
  CHECK_NE(FlowStep::PasswordChangeRequest_FlowStep_VERIFY_SUBMISSION_STEP,
           step);
  GetStepQuality(step, final_log_data_)->set_status(GetStepStatus(failure));
}

void ModelQualityLogsUploader::SetLoginPasswordFormInfo(
    const password_manager::PasswordForm& password_form) {
  optimization_guide::proto::PasswordChangeQuality* quality =
      final_log_data_.mutable_password_change_submission()->mutable_quality();
  quality->set_was_password_stored(
      password_form.in_store != password_manager::PasswordForm::Store::kNotSet);
  SetFormData(*quality->mutable_login_form_data(), password_form);
}

void ModelQualityLogsUploader::SetChangePasswordFormData(
    const password_manager::PasswordForm& password_form) {
  optimization_guide::proto::PasswordChangeQuality* quality =
      final_log_data_.mutable_password_change_submission()->mutable_quality();
  SetFormData(*quality->mutable_change_password_form_data(), password_form);
}

void ModelQualityLogsUploader::SetStepDuration(FlowStep step,
                                               base::TimeDelta duration) {
  GetStepQuality(step, final_log_data_)
      ->set_request_latency_ms(duration.InMilliseconds());
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

  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->set_total_flow_time_ms(ComputeRequestLatencyMs(flow_start_time_));
  new_log_entry->log_ai_data_request()->MergeFrom(final_log_data_);

  optimization_guide::ModelQualityLogEntry::Upload(std::move(new_log_entry));
}
