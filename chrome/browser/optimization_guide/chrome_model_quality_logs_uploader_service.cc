// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/chrome_model_quality_logs_uploader_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/channel_info.h"
#include "chrome_model_quality_logs_uploader_service.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/metrics/version_utils.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace optimization_guide {

namespace {

void RecordUploadStatusHistogram(const MqlsFeatureMetadata* metadata,
                                 ModelQualityLogsUploadStatus status) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.",
           metadata->name()}),
      status);
}

// Populates the given SystemProfileProto using the persistent system profile.
// Returns false if the persistent system profile is not available.
bool PopulatePersistentSystemProfile(
    metrics::SystemProfileProto* system_profile) {
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  if (!allocator || !allocator->memory_allocator()) {
    return false;
  }

  return metrics::PersistentSystemProfile::GetSystemProfile(
      *allocator->memory_allocator(), system_profile);
}

}  // namespace

ChromeModelQualityLogsUploaderService::ChromeModelQualityLogsUploaderService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    base::WeakPtr<ModelExecutionFeaturesController>
        model_execution_feature_controller)
    : ModelQualityLogsUploaderService(url_loader_factory, pref_service),
      model_execution_feature_controller_(model_execution_feature_controller) {}

ChromeModelQualityLogsUploaderService::
    ~ChromeModelQualityLogsUploaderService() = default;

bool ChromeModelQualityLogsUploaderService::CanUploadLogs(
    const MqlsFeatureMetadata* metadata) {
  CHECK(metadata);
  // Model quality logging requires metrics reporting to be enabled. Skip upload
  // if metrics reporting is disabled.
  if (!g_browser_process->GetMetricsServicesManager()
           ->IsMetricsConsentGiven()) {
    RecordUploadStatusHistogram(
        metadata, ModelQualityLogsUploadStatus::kMetricsReportingDisabled);
    return false;
  }

  // Don't upload logs if logging is disabled for the feature. Nothing to
  // upload.
  if (!features::IsModelQualityLoggingEnabledForFeature(metadata)) {
    RecordUploadStatusHistogram(
        metadata, ModelQualityLogsUploadStatus::kLoggingNotEnabled);
    return false;
  }

  if (model_execution_feature_controller_) {
    // Don't upload logs if the feature uses the chrome://settings/ai page
    // (i.e. has a registered UserVisibleFeatureKey) but is not enabled for the
    // user. If the feature don't use the chrome://settings/ai page, we assume
    // the feature is enabled given we have a log entry.
    std::optional<UserVisibleFeatureKey> feature_key =
        metadata->user_visible_feature_key();
    if (feature_key &&
        !model_execution_feature_controller_
             ->ShouldFeatureBeCurrentlyEnabledForUser(*feature_key)) {
      RecordUploadStatusHistogram(
          metadata, ModelQualityLogsUploadStatus::kFeatureNotEnabledForUser);
      return false;
    }

    // Don't upload logs if logging is disabled by enterprise policy.
    if (!model_execution_feature_controller_
             ->ShouldFeatureBeCurrentlyAllowedForLogging(metadata)) {
      RecordUploadStatusHistogram(
          metadata,
          ModelQualityLogsUploadStatus::kDisabledDueToEnterprisePolicy);
      return false;
    }
  }

  return true;
}

void ChromeModelQualityLogsUploaderService::SetSystemMetadata(
    proto::LoggingMetadata* logging_metadata) {
  CHECK(logging_metadata);

  // Set system profile proto before uploading. Try to use persistent system
  // profile. If that is not available, then use the core system profile (Note
  // this lacks field trial information).
  if (!PopulatePersistentSystemProfile(
          logging_metadata->mutable_system_profile())) {
    metrics::MetricsLog::RecordCoreSystemProfile(
        metrics::GetVersionString(),
        metrics::AsProtobufChannel(chrome::GetChannel()),
        chrome::IsExtendedStableChannel(),
        g_browser_process->GetApplicationLocale(), metrics::GetAppPackageName(),
        logging_metadata->mutable_system_profile());
  }
  // Remove identifiers for privacy reasons.
  logging_metadata->mutable_system_profile()->clear_client_uuid();
  logging_metadata->mutable_system_profile()->mutable_cloned_install_info()
      ->clear_cloned_from_client_id();

  auto* variations_service = g_browser_process->variations_service();
  if (variations_service) {
    logging_metadata->set_is_likely_dogfood_client(
        variations_service->IsLikelyDogfoodClient());
  }
}

}  // namespace optimization_guide
