// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/chrome_model_quality_logs_uploader_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/strings/strcat.h"
#include "build/buildflag.h"
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
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/optimization_guide_buildflags.h"
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
      local_state_(*pref_service),
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
    // Don't upload logs if logging is disabled by enterprise policy. Or, in
    // case there is no enterprise policy set, disable logging if the user is
    // enterprise.
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
  logging_metadata->mutable_system_profile()
      ->mutable_cloned_install_info()
      ->clear_cloned_from_client_id();

  auto* variations_service = g_browser_process->variations_service();
  if (variations_service) {
    logging_metadata->set_is_likely_dogfood_client(
        variations_service->IsLikelyDogfoodClient());
  }
}

proto::PerformanceClass
ChromeModelQualityLogsUploaderService::GetPerformanceClass() {
#if BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
  auto performance_class = PerformanceClassFromPref(*local_state_);
  switch (performance_class) {
    case OnDeviceModelPerformanceClass::kVeryLow:
      return proto::PERFORMANCE_CLASS_VERY_LOW;
    case OnDeviceModelPerformanceClass::kLow:
      return proto::PERFORMANCE_CLASS_LOW;
    case OnDeviceModelPerformanceClass::kMedium:
      return proto::PERFORMANCE_CLASS_MEDIUM;
    case OnDeviceModelPerformanceClass::kHigh:
      return proto::PERFORMANCE_CLASS_HIGH;
    case OnDeviceModelPerformanceClass::kVeryHigh:
      return proto::PERFORMANCE_CLASS_VERY_HIGH;
    case OnDeviceModelPerformanceClass::kUnknown:
    case OnDeviceModelPerformanceClass::kError:
    case OnDeviceModelPerformanceClass::kServiceCrash:
    case OnDeviceModelPerformanceClass::kGpuBlocked:
    case OnDeviceModelPerformanceClass::kFailedToLoadLibrary:
      return proto::PERFORMANCE_CLASS_UNSPECIFIED;
  }
#endif  // BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
  return proto::PERFORMANCE_CLASS_UNSPECIFIED;
}

}  // namespace optimization_guide
