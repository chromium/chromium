// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/actor_login_quality_logger.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

ActorLoginQualityLogger::ActorLoginQualityLogger() = default;
ActorLoginQualityLogger::~ActorLoginQualityLogger() = default;

void ActorLoginQualityLogger::SetGetCredentialsDetails(
    optimization_guide::proto::ActorLoginQuality_GetCredentialsDetails
        get_credentials_details) {
  log_data_.mutable_get_credentials_details()->CopyFrom(
      get_credentials_details);
}

void ActorLoginQualityLogger::AddAttemptLoginDetails(
    optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails
        attempt_login_details) {
  log_data_.add_attempt_login_details()->CopyFrom(attempt_login_details);
}

void ActorLoginQualityLogger::UploadFinalLog(
    optimization_guide::ModelQualityLogsUploaderService* mqls_uploader) const {
  if (!mqls_uploader) {
    return;
  }

  // TODO(crbug.com/434178974): Here, the log entry proto should be merged
  // with `log_data_request`, which will record values throughout the entire
  // login flow. To be updated when `actor_login` feature is synced in Chrome.
  optimization_guide::proto::LogAiDataRequest log_data_request;

  auto new_log_entry =
      std::make_unique<optimization_guide::ModelQualityLogEntry>(
          mqls_uploader->GetWeakPtr());
  new_log_entry->log_ai_data_request()->MergeFrom(log_data_request);

  optimization_guide::ModelQualityLogEntry::Upload(std::move(new_log_entry));
}

base::WeakPtr<ActorLoginQualityLogger> ActorLoginQualityLogger::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
