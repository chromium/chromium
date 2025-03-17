// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_client_side_detection_service_delegate.h"

#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/core/common/utils.h"

namespace {
// Currently, the following errors, which are used when a model may have been
// installed but not yet loaded, are treated as waitable.
static constexpr auto kWaitableReasons =
    base::MakeFixedFlatSet<optimization_guide::OnDeviceModelEligibilityReason>({
        optimization_guide::OnDeviceModelEligibilityReason::
            kConfigNotAvailableForFeature,
        optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled,
        optimization_guide::OnDeviceModelEligibilityReason::
            kSafetyModelNotAvailable,
        optimization_guide::OnDeviceModelEligibilityReason::
            kLanguageDetectionModelNotAvailable,
    });
}  // namespace

void LogOnDeviceModelDownloadSuccess(bool success) {
  base::UmaHistogramBoolean("SBClientPhishing.OnDeviceModelDownloadSuccess",
                            success);
}

namespace safe_browsing {

ChromeClientSideDetectionServiceDelegate::
    ChromeClientSideDetectionServiceDelegate(Profile* profile)
    : profile_(profile) {}

ChromeClientSideDetectionServiceDelegate::
    ~ChromeClientSideDetectionServiceDelegate() {
  StopListeningToOnDeviceModelUpdate();
}

PrefService* ChromeClientSideDetectionServiceDelegate::GetPrefs() {
  if (profile_) {
    return profile_->GetPrefs();
  }
  return nullptr;
}
scoped_refptr<network::SharedURLLoaderFactory>
ChromeClientSideDetectionServiceDelegate::GetURLLoaderFactory() {
  if (profile_) {
    return profile_->GetURLLoaderFactory();
  }
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeClientSideDetectionServiceDelegate::GetSafeBrowsingURLLoaderFactory() {
  if (g_browser_process->safe_browsing_service()) {
    return g_browser_process->safe_browsing_service()->GetURLLoaderFactory(
        profile_);
  }
  return nullptr;
}

bool ChromeClientSideDetectionServiceDelegate::ShouldSendModelToBrowserContext(
    content::BrowserContext* context) {
  return context == profile_;
}

void ChromeClientSideDetectionServiceDelegate::
    StartListeningToOnDeviceModelUpdate() {
  if (observing_on_device_model_availability_) {
    return;
  }

  auto* opt_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);

  if (!opt_guide) {
    return;
  }

  using ::optimization_guide::SessionConfigParams;
  SessionConfigParams config_params = SessionConfigParams{
      .execution_mode = SessionConfigParams::ExecutionMode::kOnDeviceOnly,
  };

  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session = opt_guide->StartSession(
          optimization_guide::ModelBasedCapabilityKey::kScamDetection,
          config_params);

  if (session) {
    NotifyServiceOnDeviceModelAvailable();
  } else {
    observing_on_device_model_availability_ = true;
    on_device_fetch_time_ = base::TimeTicks::Now();
    opt_guide->AddOnDeviceModelAvailabilityChangeObserver(
        optimization_guide::ModelBasedCapabilityKey::kScamDetection, this);
  }
}

void ChromeClientSideDetectionServiceDelegate::
    StopListeningToOnDeviceModelUpdate() {
  if (!observing_on_device_model_availability_) {
    return;
  }

  auto* opt_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);

  if (!opt_guide) {
    return;
  }

  observing_on_device_model_availability_ = false;
  opt_guide->RemoveOnDeviceModelAvailabilityChangeObserver(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection, this);
}

void ChromeClientSideDetectionServiceDelegate::OnDeviceModelAvailabilityChanged(
    optimization_guide::ModelBasedCapabilityKey feature,
    optimization_guide::OnDeviceModelEligibilityReason reason) {
  if (!observing_on_device_model_availability_ ||
      feature != optimization_guide::ModelBasedCapabilityKey::kScamDetection) {
    return;
  }

  if (kWaitableReasons.contains(reason)) {
    return;
  }

  if (reason == optimization_guide::OnDeviceModelEligibilityReason::kSuccess) {
    base::UmaHistogramLongTimes("SBClientPhishing.OnDeviceModelFetchTime",
                                base::TimeTicks::Now() - on_device_fetch_time_);
    NotifyServiceOnDeviceModelAvailable();
  } else {
    LogOnDeviceModelDownloadSuccess(false);
  }
}

void ChromeClientSideDetectionServiceDelegate::
    NotifyServiceOnDeviceModelAvailable() {
  LogOnDeviceModelDownloadSuccess(true);
  ClientSideDetectionService* csd_service =
      ClientSideDetectionServiceFactory::GetForProfile(profile_);
  // This can be null in unit tests.
  if (csd_service) {
    csd_service->NotifyOnDeviceModelAvailable();
  }
}

std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
ChromeClientSideDetectionServiceDelegate::GetModelExecutorSession() {
  auto* opt_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);

  if (!opt_guide) {
    return nullptr;
  }

  using ::optimization_guide::SessionConfigParams;
  SessionConfigParams config_params = SessionConfigParams{
      .execution_mode = SessionConfigParams::ExecutionMode::kOnDeviceOnly,
      .logging_mode = SessionConfigParams::LoggingMode::kDefault,
  };

  return opt_guide->StartSession(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      config_params);
}

void ChromeClientSideDetectionServiceDelegate::
    LogOnDeviceModelEligibilityReason() {
  auto* opt_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);

  if (!opt_guide) {
    return;
  }

  optimization_guide::OnDeviceModelEligibilityReason eligibility =
      opt_guide->GetOnDeviceModelEligibility(
          optimization_guide::ModelBasedCapabilityKey::kScamDetection);
  base::UmaHistogramEnumeration(
      "SBClientPhishing.OnDeviceModelEligibilityReasonAtInquiryFailure",
      eligibility);
}

}  // namespace safe_browsing
