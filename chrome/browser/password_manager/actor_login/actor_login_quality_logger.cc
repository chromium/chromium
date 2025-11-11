// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/actor_login_quality_logger.h"

#include <memory>

#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

ActorLoginQualityLogger::ActorLoginQualityLogger() = default;
ActorLoginQualityLogger::~ActorLoginQualityLogger() = default;

void ActorLoginQualityLogger::UploadFinalLog(
    optimization_guide::ModelQualityLogsUploaderService* mqls_uploader) {
  if (!mqls_uploader) {
    return;
  }
  auto new_log_entry =
      std::make_unique<optimization_guide::ModelQualityLogEntry>(
          mqls_uploader->GetWeakPtr());

  // TODO(crbug.com/434178974): Here, the log entry proto should be merged
  // with `log_data`, which will record values throughout the entire
  // login flow. To be updated when `actor_login` feature is synced in Chrome.
  new_log_entry->log_ai_data_request()->MergeFrom(log_data_);

  optimization_guide::ModelQualityLogEntry::Upload(std::move(new_log_entry));
}
