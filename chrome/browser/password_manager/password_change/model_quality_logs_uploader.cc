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
using PasswordChangeOutcome = optimization_guide::proto ::
    PasswordChangeSubmissionData_PasswordChangeOutcome;

ModelQualityLogsUploader::ModelQualityLogsUploader(Profile* profile)
    : profile_(profile) {}
ModelQualityLogsUploader::~ModelQualityLogsUploader() = default;

void ModelQualityLogsUploader::AddFinalModelStatusLog(
    FinalModelStatus final_model_status,
    std::unique_ptr<
        optimization_guide::proto::PasswordChangeSubmissionLoggingData>
        logging_data) {
  optimization_guide::proto::LogAiDataRequest request;
  request.mutable_password_change_submission()->MergeFrom(*logging_data);
  // Set final model status
  request.mutable_password_change_submission()
      ->mutable_quality()
      ->set_final_model_status(final_model_status);
  // Store the new log request
  log_entries_requests_.push_back(std::move(request));
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
  for (const auto& entry : log_entries_requests_) {
    new_log_entry->log_ai_data_request()->MergeFrom(entry);
  }
  optimization_guide::ModelQualityLogEntry::Upload(std::move(new_log_entry));
}

void ModelQualityLogsUploader::MergeData(
    const optimization_guide::proto::PasswordChangeResponse& response,
    std::unique_ptr<
        optimization_guide::proto::PasswordChangeSubmissionLoggingData>
        logging_data) {
  // TODO(407503334): Split this per step, now assuming it is just verify
  // submission.
  PasswordChangeOutcome outcome = response.outcome_data().submission_outcome();
  if (outcome !=
          PasswordChangeOutcome::
              PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME &&
      outcome !=
          PasswordChangeOutcome::
              PasswordChangeSubmissionData_PasswordChangeOutcome_UNKNOWN_OUTCOME) {
    AddFinalModelStatusLog(FinalModelStatus::FINAL_MODEL_STATUS_FAILURE,
                           std::move(logging_data));
  } else {
    AddFinalModelStatusLog(FinalModelStatus::FINAL_MODEL_STATUS_SUCCESS,
                           std::move(logging_data));
  }
}
