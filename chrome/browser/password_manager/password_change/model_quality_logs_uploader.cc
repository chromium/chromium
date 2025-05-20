// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"

#include "base/logging.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "content/public/browser/web_contents.h"

using FinalModelStatus = optimization_guide::proto::FinalModelStatus;
using QualityStatus = optimization_guide::proto::
    PasswordChangeQuality_StepQuality_SubmissionStatus;
using PasswordChangeOutcome = optimization_guide::proto ::
    PasswordChangeSubmissionData_PasswordChangeOutcome;
using PageType = optimization_guide::proto::OpenFormResponseData_PageType;

namespace {
int64_t ComputeRequestLatencyMs(base::Time server_request_start_time) {
  return (base::Time::Now() - server_request_start_time).InMilliseconds();
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

QualityStatus GetVerifySubmissionQualityStatus(
    const std::optional<optimization_guide::proto::PasswordChangeResponse>&
        response) {
  if (!response.has_value()) {
    return QualityStatus::
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
    return QualityStatus::
        PasswordChangeQuality_StepQuality_SubmissionStatus_FAILURE_STATUS;
  }
  return QualityStatus::
      PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS;
}
}  // namespace

ModelQualityLogsUploader::ModelQualityLogsUploader(Profile* profile)
    : profile_(profile) {}
ModelQualityLogsUploader::~ModelQualityLogsUploader() = default;

void ModelQualityLogsUploader::SetOpenFormQuality(
    const optimization_guide::proto::PasswordChangeResponse& response,
    std::unique_ptr<LoggingData> logging_data,
    base::Time server_request_start_time) {
  PageType open_form = response.open_form_data().page_type();
  QualityStatus quality_status;

  if (open_form == PageType::OpenFormResponseData_PageType_SETTINGS_PAGE) {
    if (response.open_form_data().dom_node_id_to_click()) {
      // Assume success at this point, if fails to actuate on it the state
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

void ModelQualityLogsUploader::OpenFormTargetElementNotFound() {
  final_log_data_.mutable_password_change_submission()
      ->mutable_quality()
      ->mutable_open_form()
      ->set_status(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND);
}

void ModelQualityLogsUploader::SetSubmitFormQuality(
    const optimization_guide::proto::PasswordChangeResponse& response,
    std::unique_ptr<LoggingData> logging_data,
    base::Time server_request_start_time) {
  QualityStatus quality_status;
  if (response.submit_form_data().dom_node_id_to_click()) {
    quality_status = QualityStatus::
        PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS;
  } else {
    quality_status = QualityStatus::
        PasswordChangeQuality_StepQuality_SubmissionStatus_ELEMENT_NOT_FOUND;
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

void ModelQualityLogsUploader::UploadFinalLog() {
  auto* logs_uploader =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_)
          ->GetModelQualityLogsUploaderService();
  if (!logs_uploader) {
    return;
  }
  auto new_log_entry =
      std::make_unique<optimization_guide::ModelQualityLogEntry>(
          logs_uploader->GetWeakPtr());

  new_log_entry->log_ai_data_request()->MergeFrom(final_log_data_);
  optimization_guide::ModelQualityLogEntry::Upload(std::move(new_log_entry));
}
