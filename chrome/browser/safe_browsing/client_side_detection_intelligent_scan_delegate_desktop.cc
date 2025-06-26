// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_desktop.h"

#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

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

void LogOnDeviceModelDownloadSuccess(bool success) {
  base::UmaHistogramBoolean("SBClientPhishing.OnDeviceModelDownloadSuccess",
                            success);
}
}  // namespace

namespace safe_browsing {

ClientSideDetectionIntelligentScanDelegateDesktop::
    ClientSideDetectionIntelligentScanDelegateDesktop(
        PrefService& pref,
        OptimizationGuideKeyedService* opt_guide)
    : pref_(pref), opt_guide_(opt_guide) {}

ClientSideDetectionIntelligentScanDelegateDesktop::
    ~ClientSideDetectionIntelligentScanDelegateDesktop() = default;

bool ClientSideDetectionIntelligentScanDelegateDesktop::
    ShouldRequestIntelligentScan(ClientPhishingRequest* verdict) {
  if (!IsEnhancedProtectionEnabled(*pref_)) {
    return false;
  }

  bool is_keyboard_lock_requested =
      base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection) &&
      verdict->client_side_detection_type() ==
          ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED;

  bool is_intelligent_scan_requested =
      base::FeatureList::IsEnabled(
          kClientSideDetectionLlamaForcedTriggerInfoForScamDetection) &&
      verdict->has_llama_forced_trigger_info() &&
      verdict->llama_forced_trigger_info().intelligent_scan();

  return is_keyboard_lock_requested || is_intelligent_scan_requested;
}

bool ClientSideDetectionIntelligentScanDelegateDesktop::
    IsOnDeviceModelAvailable(bool log_failed_eligibility_reason) {
  if (log_failed_eligibility_reason && !on_device_model_available_) {
    LogOnDeviceModelEligibilityReason();
  }
  return on_device_model_available_;
}

void ClientSideDetectionIntelligentScanDelegateDesktop::
    StartListeningToOnDeviceModelUpdate() {
  if (observing_on_device_model_availability_) {
    return;
  }

  using ::optimization_guide::SessionConfigParams;
  SessionConfigParams config_params = SessionConfigParams{
      .execution_mode = SessionConfigParams::ExecutionMode::kOnDeviceOnly,
  };

  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session = opt_guide_->StartSession(
          optimization_guide::ModelBasedCapabilityKey::kScamDetection,
          config_params);

  if (session) {
    NotifyOnDeviceModelAvailable();
  } else {
    observing_on_device_model_availability_ = true;
    on_device_fetch_time_ = base::TimeTicks::Now();
    opt_guide_->AddOnDeviceModelAvailabilityChangeObserver(
        optimization_guide::ModelBasedCapabilityKey::kScamDetection, this);
  }
}

void ClientSideDetectionIntelligentScanDelegateDesktop::
    StopListeningToOnDeviceModelUpdate() {
  on_device_model_available_ = false;
  if (!observing_on_device_model_availability_) {
    return;
  }

  observing_on_device_model_availability_ = false;
  opt_guide_->RemoveOnDeviceModelAvailabilityChangeObserver(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection, this);
}

void ClientSideDetectionIntelligentScanDelegateDesktop::Shutdown() {
  StopListeningToOnDeviceModelUpdate();
}

void ClientSideDetectionIntelligentScanDelegateDesktop::
    OnDeviceModelAvailabilityChanged(
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
    NotifyOnDeviceModelAvailable();
  } else {
    LogOnDeviceModelDownloadSuccess(false);
  }
}

void ClientSideDetectionIntelligentScanDelegateDesktop::
    NotifyOnDeviceModelAvailable() {
  LogOnDeviceModelDownloadSuccess(true);
  on_device_model_available_ = true;
}

void ClientSideDetectionIntelligentScanDelegateDesktop::
    LogOnDeviceModelEligibilityReason() {
  optimization_guide::OnDeviceModelEligibilityReason eligibility =
      opt_guide_->GetOnDeviceModelEligibility(
          optimization_guide::ModelBasedCapabilityKey::kScamDetection);
  base::UmaHistogramEnumeration(
      "SBClientPhishing.OnDeviceModelEligibilityReasonAtInquiryFailure",
      eligibility);
}

}  // namespace safe_browsing
