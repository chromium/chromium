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
#include "content/public/browser/browser_thread.h"

namespace {
using ScamDetectionRequest = optimization_guide::proto::ScamDetectionRequest;
using ScamDetectionResponse = optimization_guide::proto::ScamDetectionResponse;

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

void LogOnDeviceModelSessionCreationSuccess(bool success) {
  base::UmaHistogramBoolean(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", success);
}

void LogOnDeviceModelExecutionSuccessAndTime(
    bool success,
    base::TimeTicks session_execution_start_time) {
  base::UmaHistogramBoolean("SBClientPhishing.OnDeviceModelExecutionSuccess",
                            success);
  base::UmaHistogramMediumTimes(
      "SBClientPhishing.OnDeviceModelExecutionDuration",
      base::TimeTicks::Now() - session_execution_start_time);
}

void LogOnDeviceModelExecutionParse(bool success) {
  base::UmaHistogramBoolean(
      "SBClientPhishing.OnDeviceModelResponseParseSuccess", success);
}

void LogOnDeviceModelCallbackStateOnSuccessfulResponse(bool is_alive) {
  base::UmaHistogramBoolean(
      "SBClientPhishing.OnDeviceModelSuccessfulResponseCallbackAlive",
      is_alive);
}
}  // namespace

namespace safe_browsing {

ClientSideDetectionIntelligentScanDelegateDesktop::
    ClientSideDetectionIntelligentScanDelegateDesktop(
        PrefService& pref,
        OptimizationGuideKeyedService* opt_guide)
    : pref_(pref), opt_guide_(opt_guide) {
  pref_change_registrar_.Init(&pref);
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(
          &ClientSideDetectionIntelligentScanDelegateDesktop::OnPrefsUpdated,
          base::Unretained(this)));
  //  Do an initial check of the prefs.
  OnPrefsUpdated();
}

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

void ClientSideDetectionIntelligentScanDelegateDesktop::OnPrefsUpdated() {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    return;
  }
  bool is_feature_enabled =
      base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection) ||
      base::FeatureList::IsEnabled(
          kClientSideDetectionLlamaForcedTriggerInfoForScamDetection);
  if (IsEnhancedProtectionEnabled(*pref_) && is_feature_enabled) {
    StartListeningToOnDeviceModelUpdate();
  } else {
    StopListeningToOnDeviceModelUpdate();
  }
}

void ClientSideDetectionIntelligentScanDelegateDesktop::InquireOnDeviceModel(
    std::string rendered_texts,
    InquireOnDeviceModelDoneCallback callback) {
  // We have checked the model availability prior to calling this function, but
  // we want to check one last time before creating a session.
  if (!IsOnDeviceModelAvailable(/*log_failed_eligibility_reason=*/false)) {
    ClientSideDetectionHost::IntelligentScanDelegate::IntelligentScanResult
        intelligent_scan_result;
    intelligent_scan_result.execution_success = false;
    std::move(callback).Run(intelligent_scan_result);
    return;
  }

  // The caller of this function is responsible for calling ResetOnDeviceSession
  // before calling this function again.
  CHECK(!session_);

  base::TimeTicks session_creation_start_time = base::TimeTicks::Now();

  session_ = GetModelExecutorSession();

  if (!session_) {
    LogOnDeviceModelSessionCreationSuccess(false);
    ClientSideDetectionHost::IntelligentScanDelegate::IntelligentScanResult
        intelligent_scan_result;
    intelligent_scan_result.execution_success = false;
    std::move(callback).Run(intelligent_scan_result);
    return;
  }

  base::UmaHistogramMediumTimes(
      "SBClientPhishing.OnDeviceModelSessionCreationTime",
      base::TimeTicks::Now() - session_creation_start_time);
  LogOnDeviceModelSessionCreationSuccess(true);

  ScamDetectionRequest request;
  request.set_rendered_text(rendered_texts);

  inquire_on_device_model_callback_ = std::move(callback);
  session_execution_start_time_ = base::TimeTicks::Now();
  session_->ExecuteModel(
      *std::make_unique<ScamDetectionRequest>(request),
      base::BindRepeating(&ClientSideDetectionIntelligentScanDelegateDesktop::
                              ModelExecutionCallback,
                          weak_factory_.GetWeakPtr()));
}

void ClientSideDetectionIntelligentScanDelegateDesktop::ModelExecutionCallback(
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  ClientSideDetectionHost::IntelligentScanDelegate::IntelligentScanResult
      intelligent_scan_result;
  if (result.execution_info &&
      result.execution_info->has_on_device_model_execution_info() &&
      result.execution_info->on_device_model_execution_info()
          .has_model_versions() &&
      result.execution_info->on_device_model_execution_info()
          .model_versions()
          .has_on_device_model_service_version()) {
    intelligent_scan_result.model_version =
        result.execution_info->on_device_model_execution_info()
            .model_versions()
            .on_device_model_service_version()
            .model_adaptation_version();
  } else {
    intelligent_scan_result.model_version = -1;
  }

  if (!result.response.has_value()) {
    LogOnDeviceModelExecutionSuccessAndTime(/*success=*/false,
                                            session_execution_start_time_);
    if (inquire_on_device_model_callback_) {
      intelligent_scan_result.execution_success = false;
      std::move(inquire_on_device_model_callback_).Run(intelligent_scan_result);
    }
    return;
  }

  // This is a non-error response, but it's not completed, yet so we wait till
  // it's complete. We will not respond to the callback yet because of this.
  if (!result.response->is_complete) {
    return;
  }

  LogOnDeviceModelExecutionSuccessAndTime(/*success=*/true,
                                          session_execution_start_time_);

  auto scam_detection_response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ScamDetectionResponse>(
      result.response->response);

  if (!scam_detection_response) {
    LogOnDeviceModelExecutionParse(false);
    if (inquire_on_device_model_callback_) {
      intelligent_scan_result.execution_success = false;
      std::move(inquire_on_device_model_callback_).Run(intelligent_scan_result);
    }
    return;
  }

  LogOnDeviceModelExecutionParse(true);

  // Reset session immediately so that future inference is not affected by the
  // old context.
  ResetOnDeviceSession();

  LogOnDeviceModelCallbackStateOnSuccessfulResponse(
      !!inquire_on_device_model_callback_);
  if (inquire_on_device_model_callback_) {
    intelligent_scan_result.brand = scam_detection_response->brand();
    intelligent_scan_result.intent = scam_detection_response->intent();
    intelligent_scan_result.execution_success = true;
    std::move(inquire_on_device_model_callback_).Run(intelligent_scan_result);
  }
}

bool ClientSideDetectionIntelligentScanDelegateDesktop::ResetOnDeviceSession() {
  bool did_reset_session = !!session_;
  if (session_) {
    session_.reset();
  }
  return did_reset_session;
}

void ClientSideDetectionIntelligentScanDelegateDesktop::
    StartListeningToOnDeviceModelUpdate() {
  if (observing_on_device_model_availability_) {
    return;
  }

  auto session = GetModelExecutorSession();

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
  ResetOnDeviceSession();
  if (!observing_on_device_model_availability_) {
    return;
  }

  observing_on_device_model_availability_ = false;
  opt_guide_->RemoveOnDeviceModelAvailabilityChangeObserver(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection, this);
}

void ClientSideDetectionIntelligentScanDelegateDesktop::Shutdown() {
  StopListeningToOnDeviceModelUpdate();
  pref_change_registrar_.RemoveAll();
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

std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
ClientSideDetectionIntelligentScanDelegateDesktop::GetModelExecutorSession() {
  using ::optimization_guide::SessionConfigParams;
  SessionConfigParams config_params = SessionConfigParams{
      .execution_mode = SessionConfigParams::ExecutionMode::kOnDeviceOnly,
      .logging_mode = SessionConfigParams::LoggingMode::kDefault,
  };

  return opt_guide_->StartSession(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      config_params);
}
}  // namespace safe_browsing
