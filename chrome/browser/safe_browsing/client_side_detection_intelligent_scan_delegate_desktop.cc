// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_desktop.h"

#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
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

void LogOnDeviceModelSessionCreationSuccess(bool success) {
  base::UmaHistogramBoolean(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", success);
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

class ClientSideDetectionIntelligentScanDelegateDesktop::Inquiry {
 public:
  Inquiry(ClientSideDetectionIntelligentScanDelegateDesktop* parent,
          const base::UnguessableToken& session_id,
          InquireOnDeviceModelDoneCallback callback);
  ~Inquiry();

  void Start(const std::string& rendered_texts);

 private:
  void ModelExecutionCallback(
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  const raw_ptr<ClientSideDetectionIntelligentScanDelegateDesktop> parent_;
  std::unique_ptr<optimization_guide::OnDeviceSession> session_;
  base::UnguessableToken session_id_;
  InquireOnDeviceModelDoneCallback callback_;
  std::string rendered_texts_;
  base::TimeTicks session_execution_start_time_;

  base::WeakPtrFactory<Inquiry> weak_factory_{this};
};

ClientSideDetectionIntelligentScanDelegateDesktop::Inquiry::Inquiry(
    ClientSideDetectionIntelligentScanDelegateDesktop* parent,
    const base::UnguessableToken& session_id,
    InquireOnDeviceModelDoneCallback callback)
    : parent_(parent),
      session_id_(session_id),
      callback_(std::move(callback)) {}

ClientSideDetectionIntelligentScanDelegateDesktop::Inquiry::~Inquiry() =
    default;

void ClientSideDetectionIntelligentScanDelegateDesktop::Inquiry::Start(
    const std::string& rendered_texts) {
  session_ = parent_->GetModelExecutorSession();

  base::TimeTicks session_creation_start_time = base::TimeTicks::Now();

  if (!session_) {
    LogOnDeviceModelSessionCreationSuccess(false);
    std::move(callback_).Run(IntelligentScanResult::Failure(
        IntelligentScanResult::kModelVersionUnavailable));
    return;
  }

  client_side_detection::LogOnDeviceModelSessionCreationTime(
      session_creation_start_time);
  LogOnDeviceModelSessionCreationSuccess(true);

  ScamDetectionRequest request;
  request.set_rendered_text(rendered_texts);

  session_execution_start_time_ = base::TimeTicks::Now();
  session_->ExecuteModel(
      *std::make_unique<ScamDetectionRequest>(request),
      base::BindRepeating(&ClientSideDetectionIntelligentScanDelegateDesktop::
                              Inquiry::ModelExecutionCallback,
                          weak_factory_.GetWeakPtr()));
}

void ClientSideDetectionIntelligentScanDelegateDesktop::Inquiry::
    ModelExecutionCallback(
        optimization_guide::OptimizationGuideModelStreamingExecutionResult
            result) {
  int model_version = IntelligentScanResult::kModelVersionUnavailable;
  if (result.execution_info) {
    model_version = result.execution_info->on_device_model_execution_info()
                        .model_versions()
                        .on_device_model_service_version()
                        .model_adaptation_version();
  }

  if (!result.response.has_value()) {
    client_side_detection::LogOnDeviceModelExecutionSuccessAndTime(
        /*success=*/false, session_execution_start_time_);
    if (callback_) {
      std::move(callback_).Run(IntelligentScanResult::Failure(model_version));
    }
    return;
  }

  // This is a non-error response, but it's not completed, yet so we wait till
  // it's complete. We will not respond to the callback yet because of this.
  if (!result.response->is_complete) {
    return;
  }

  client_side_detection::LogOnDeviceModelExecutionSuccessAndTime(
      /*success=*/true, session_execution_start_time_);

  auto scam_detection_response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ScamDetectionResponse>(
      result.response->response);

  if (!scam_detection_response) {
    LogOnDeviceModelExecutionParse(false);
    if (callback_) {
      std::move(callback_).Run(IntelligentScanResult::Failure(model_version));
    }
    return;
  }

  LogOnDeviceModelExecutionParse(true);
  LogOnDeviceModelCallbackStateOnSuccessfulResponse(!!callback_);

  if (callback_) {
    std::move(callback_).Run({.brand = scam_detection_response->brand(),
                              .intent = scam_detection_response->intent(),
                              .model_version = model_version,
                              .execution_success = true});
  }

  // Reset session immediately so that future inference is not affected by the
  // old context.
  parent_->CancelSession(session_id_);
}

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

bool ClientSideDetectionIntelligentScanDelegateDesktop::ShouldShowScamWarning(
    std::optional<IntelligentScanVerdict> verdict) {
  if (!verdict.has_value() ||
      *verdict ==
          IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_UNSPECIFIED ||
      *verdict == IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE) {
    return false;
  }

  return *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1 ||
         (base::FeatureList::IsEnabled(
              kClientSideDetectionShowLlamaScamVerdictWarning) &&
          *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2) ||
         *verdict ==
             IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT;
}

void ClientSideDetectionIntelligentScanDelegateDesktop::OnPrefsUpdated() {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    return;
  }

  if (IsEnhancedProtectionEnabled(*pref_)) {
    StartListeningToOnDeviceModelUpdate();
  } else {
    StopListeningToOnDeviceModelUpdate();
  }
}

std::optional<base::UnguessableToken>
ClientSideDetectionIntelligentScanDelegateDesktop::InquireOnDeviceModel(
    std::string rendered_texts,
    InquireOnDeviceModelDoneCallback callback) {
  // We have checked the model availability prior to calling this function, but
  // we want to check one last time before creating a session.
  if (!IsOnDeviceModelAvailable(/*log_failed_eligibility_reason=*/false)) {
    std::move(callback).Run(IntelligentScanResult::Failure(
        IntelligentScanResult::kModelVersionUnavailable));
    return std::nullopt;
  }

  base::UnguessableToken session_id = base::UnguessableToken::Create();
  std::unique_ptr<Inquiry> new_inquiry =
      std::make_unique<Inquiry>(this, session_id, std::move(callback));
  inquiries_[session_id] = std::move(new_inquiry);
  inquiries_[session_id]->Start(rendered_texts);
  return session_id;
}

bool ClientSideDetectionIntelligentScanDelegateDesktop::CancelSession(
    const base::UnguessableToken& session_id) {
  if (!inquiries_.contains(session_id)) {
    return false;
  }

  inquiries_.erase(session_id);
  return true;
}

bool ClientSideDetectionIntelligentScanDelegateDesktop::ResetAllSessions() {
  bool did_reset_session = !inquiries_.empty();
  inquiries_.clear();
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
        optimization_guide::mojom::OnDeviceFeature::kScamDetection, this);
  }
}

void ClientSideDetectionIntelligentScanDelegateDesktop::
    StopListeningToOnDeviceModelUpdate() {
  on_device_model_available_ = false;
  ResetAllSessions();
  if (!observing_on_device_model_availability_) {
    return;
  }

  observing_on_device_model_availability_ = false;
  opt_guide_->RemoveOnDeviceModelAvailabilityChangeObserver(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection, this);
}

void ClientSideDetectionIntelligentScanDelegateDesktop::Shutdown() {
  client_side_detection::LogOnDeviceModelSessionAliveOnDelegateShutdown(
      !inquiries_.empty());
  StopListeningToOnDeviceModelUpdate();
  pref_change_registrar_.RemoveAll();
}

void ClientSideDetectionIntelligentScanDelegateDesktop::
    OnDeviceModelAvailabilityChanged(
        optimization_guide::mojom::OnDeviceFeature feature,
        optimization_guide::OnDeviceModelEligibilityReason reason) {
  if (!observing_on_device_model_availability_ ||
      feature != optimization_guide::mojom::OnDeviceFeature::kScamDetection) {
    return;
  }

  if (kWaitableReasons.contains(reason)) {
    return;
  }

  if (reason == optimization_guide::OnDeviceModelEligibilityReason::kSuccess) {
    client_side_detection::LogOnDeviceModelFetchTime(on_device_fetch_time_);
    NotifyOnDeviceModelAvailable();
  } else {
    client_side_detection::LogOnDeviceModelDownloadSuccess(false);
  }
}

void ClientSideDetectionIntelligentScanDelegateDesktop::
    NotifyOnDeviceModelAvailable() {
  client_side_detection::LogOnDeviceModelDownloadSuccess(true);
  on_device_model_available_ = true;
}

void ClientSideDetectionIntelligentScanDelegateDesktop::
    LogOnDeviceModelEligibilityReason() {
  optimization_guide::OnDeviceModelEligibilityReason eligibility =
      opt_guide_->GetOnDeviceModelEligibility(
          optimization_guide::mojom::OnDeviceFeature::kScamDetection);
  base::UmaHistogramEnumeration(
      "SBClientPhishing.OnDeviceModelEligibilityReasonAtInquiryFailure",
      eligibility);
}

std::unique_ptr<optimization_guide::OnDeviceSession>
ClientSideDetectionIntelligentScanDelegateDesktop::GetModelExecutorSession() {
  return opt_guide_->StartSession(
      optimization_guide::mojom::OnDeviceFeature::kScamDetection,
      ::optimization_guide::SessionConfigParams{}, nullptr);
}
}  // namespace safe_browsing
