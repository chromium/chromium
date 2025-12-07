// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/actor_login_quality_logger.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"

ActorLoginQualityLogger::ActorLoginQualityLogger() {
  variations::VariationsService* variation_service =
      g_browser_process->variations_service();
  if (variation_service) {
    log_data_.mutable_actor_login()->mutable_quality()->set_location(
        base::ToUpperASCII(variation_service->GetLatestCountry()));
  }
}

ActorLoginQualityLogger::~ActorLoginQualityLogger() = default;

void ActorLoginQualityLogger::SetDomainAndLanguage(
    translate::TranslateManager* translate_manager,
    const GURL& url) {
  // This should only be set once per log entry, by the first
  // request.
  if (log_data_.mutable_actor_login()->mutable_quality()->has_domain()) {
    return;
  }
  log_data_.mutable_actor_login()->mutable_quality()->set_domain(
      affiliations::GetExtendedTopLevelDomain(url,
                                              /*psl_extensions=*/{}));
  if (translate_manager) {
    log_data_.mutable_actor_login()->mutable_quality()->set_language(
        translate_manager->GetLanguageState()->source_language());
  }
}

void ActorLoginQualityLogger::SetGetCredentialsDetails(
    optimization_guide::proto::ActorLoginQuality_GetCredentialsDetails
        get_credentials_details) {
  log_data_.mutable_actor_login()
      ->mutable_quality()
      ->mutable_get_credentials_details()
      ->CopyFrom(get_credentials_details);
}

void ActorLoginQualityLogger::AddAttemptLoginDetails(
    optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails
        attempt_login_details) {
  log_data_.mutable_actor_login()
      ->mutable_quality()
      ->add_attempt_login_details()
      ->CopyFrom(attempt_login_details);
}

void ActorLoginQualityLogger::SetPermissionPicked(
    optimization_guide::proto::ActorLoginQuality_PermissionOption
        permission_option) {
  log_data_.mutable_actor_login()->mutable_quality()->set_permission_picked(
      permission_option);
}

void ActorLoginQualityLogger::UploadFinalLog(
    optimization_guide::ModelQualityLogsUploaderService* mqls_uploader) const {
  if (!mqls_uploader) {
    return;
  }

  auto new_log_entry =
      std::make_unique<optimization_guide::ModelQualityLogEntry>(
          mqls_uploader->GetWeakPtr());
  new_log_entry->log_ai_data_request()->MergeFrom(log_data_);
  optimization_guide::ModelQualityLogEntry::Upload(std::move(new_log_entry));
}

base::WeakPtr<ActorLoginQualityLogger> ActorLoginQualityLogger::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
