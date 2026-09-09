// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_desktop.h"

#include "base/containers/fixed_flat_set.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"

namespace {
using ScamDetectionRequest = optimization_guide::proto::ScamDetectionRequest;
using ScamDetectionResponse = optimization_guide::proto::ScamDetectionResponse;
using ModelType = safe_browsing::IntelligentScanDelegate::ModelType;

// Intelligent scan is always performed on the on-device model on desktop.
constexpr auto kOnDeviceModelType =
    safe_browsing::IntelligentScanDelegate::ModelType::kOnDevice;

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
          const base::UnguessableToken& scan_id,
          IntelligentScanDoneCallback callback);
  ~Inquiry();

  void Start(const std::string& rendered_texts);

 private:
  void ModelExecutionCallback(
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);
  void RemoteExecutionCallback(
      base::TimeTicks remote_execution_start_time,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>);

  const raw_ptr<ClientSideDetectionIntelligentScanDelegateDesktop> parent_;
  std::unique_ptr<optimization_guide::OnDeviceSession> session_;
  base::UnguessableToken scan_id_;
  IntelligentScanDoneCallback callback_;
  std::string rendered_texts_;
  base::TimeTicks session_execution_start_time_;

  base::WeakPtrFactory<Inquiry> weak_factory_{this};
};

ClientSideDetectionIntelligentScanDelegateDesktop::Inquiry::Inquiry(
    ClientSideDetectionIntelligentScanDelegateDesktop* parent,
    const base::UnguessableToken& scan_id,
    IntelligentScanDoneCallback callback)
    : parent_(parent), scan_id_(scan_id), callback_(std::move(callback)) {}

ClientSideDetectionIntelligentScanDelegateDesktop::Inquiry::~Inquiry() =
    default;

void ClientSideDetectionIntelligentScanDelegateDesktop::Inquiry::Start(
    const std::string& rendered_texts) {
  if (parent_->is_server_model_enabled_) {
    parent_->AddIntelligentScanQuota();
    ScamDetectionRequest request;
    request.set_rendered_text(rendered_texts);
    parent_->remote_model_executor_->ExecuteModel(
        optimization_guide::ModelBasedCapabilityKey::kScamDetection, request,
        /*options=*/{},
        base::BindOnce(&ClientSideDetectionIntelligentScanDelegateDesktop::
                           Inquiry::RemoteExecutionCallback,
                       weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
    // Do not access `parent_` at this point. The callback may be called
    // immediately and this object will delete itself.
    return;
  }

  session_ = parent_->GetModelExecutorSession();

  base::TimeTicks session_creation_start_time = base::TimeTicks::Now();

  if (!session_) {
    LogOnDeviceModelSessionCreationSuccess(false);
    std::move(callback_).Run(IntelligentScanResult::Failure(
        IntelligentScanResult::kModelVersionUnavailable, kOnDeviceModelType,
        IntelligentScanInfo::ON_DEVICE_MODEL_OUTPUT_MISSING));
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
      std::move(callback_).Run(IntelligentScanResult::Failure(
          model_version, kOnDeviceModelType,
          IntelligentScanInfo::ON_DEVICE_MODEL_OUTPUT_MISSING));
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
      std::move(callback_).Run(IntelligentScanResult::Failure(
          model_version, kOnDeviceModelType,
          IntelligentScanInfo::ON_DEVICE_MODEL_OUTPUT_MISSING));
    }
    return;
  }

  LogOnDeviceModelExecutionParse(true);
  LogOnDeviceModelCallbackStateOnSuccessfulResponse(!!callback_);

  if (callback_) {
    std::optional<float> scam_score = std::nullopt;
    if (scam_detection_response->has_scam_score()) {
      scam_score = scam_detection_response->scam_score();
    }
    std::move(callback_).Run(IntelligentScanResult::Success(
        scam_detection_response->brand(), scam_detection_response->intent(),
        model_version, kOnDeviceModelType, scam_score));
  }

  // Reset inquiry immediately so that future inference is not affected by the
  // old context.
  parent_->CancelIntelligentScan(scan_id_);
}

void ClientSideDetectionIntelligentScanDelegateDesktop::Inquiry::
    RemoteExecutionCallback(
        base::TimeTicks remote_execution_start_time,
        optimization_guide::OptimizationGuideModelExecutionResult result,
        std::unique_ptr<optimization_guide::ModelQualityLogEntry>) {
  CHECK(callback_);
  bool execution_success = result.response.has_value();
  base::UmaHistogramBoolean("SBClientPhishing.ServerSideModelExecutionSuccess",
                            execution_success);
  base::UmaHistogramMediumTimes(
      "SBClientPhishing.ServerSideModelExecutionDuration",
      base::TimeTicks::Now() - remote_execution_start_time);
  // Server model does not return model version. Check the rollout feature flag
  // to set the model version.
  int model_version =
      base::FeatureList::IsEnabled(
          kClientSideDetectionServerModelRolloutDesktop)
          ? kClientSideDetectionServerModelRolloutVersionDesktop.Get()
          : IntelligentScanResult::kDefaultServerModelVersion;
  if (!execution_success) {
    base::UmaHistogramEnumeration(
        "SBClientPhishing.ServerSideModelExecutionError",
        result.response.error().error());
    std::move(callback_).Run(IntelligentScanResult::Failure(
        model_version, ModelType::kServerSide,
        IntelligentScanInfo::SERVER_SIDE_MODEL_OUTPUT_MISSING));
    parent_->CancelIntelligentScan(scan_id_);
    return;
  }

  auto scam_detection_response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ScamDetectionResponse>(
      result.response.value());

  if (!scam_detection_response) {
    std::move(callback_).Run(IntelligentScanResult::Failure(
        model_version, ModelType::kServerSide,
        IntelligentScanInfo::SERVER_SIDE_MODEL_OUTPUT_MISSING));
    parent_->CancelIntelligentScan(scan_id_);
    return;
  }
  std::optional<float> scam_score;
  if (scam_detection_response->has_scam_score()) {
    scam_score = scam_detection_response->scam_score();
  }
  std::move(callback_).Run(IntelligentScanResult::Success(
      scam_detection_response->brand(), scam_detection_response->intent(),
      model_version, ModelType::kServerSide, scam_score));

  // Reset this inquiry immediately so that future inference is not affected by
  // the old context.
  parent_->CancelIntelligentScan(scan_id_);
}

ClientSideDetectionIntelligentScanDelegateDesktop::
    ClientSideDetectionIntelligentScanDelegateDesktop(
        PrefService& pref,
        OptimizationGuideKeyedService* opt_guide,
        policy::ManagementService* management_service,
        optimization_guide::RemoteModelExecutor* remote_model_executor)
    : pref_(pref),
      opt_guide_(opt_guide),
      management_service_(management_service),
      remote_model_executor_(remote_model_executor),
      is_feature_enabled_(
          !base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)),
      is_server_model_enabled_(base::FeatureList::IsEnabled(
          kClientSideDetectionServerModelForScamDetectionDesktop)) {
  if (!is_feature_enabled_) {
    return;
  }
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
  if (!is_feature_enabled_) {
    return false;
  }

  if (!IsEnhancedProtectionEnabled(*pref_)) {
    return false;
  }

  bool is_keyboard_lock_requested =
      verdict->client_side_detection_type() ==
      ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED;

  bool is_intelligent_scan_requested =
      verdict->client_side_detection_type() ==
          ClientSideDetectionType::FORCE_REQUEST &&
      verdict->has_llama_forced_trigger_info() &&
      verdict->llama_forced_trigger_info().intelligent_scan();

  return is_keyboard_lock_requested || is_intelligent_scan_requested;
}

ModelType
ClientSideDetectionIntelligentScanDelegateDesktop::GetIntelligentScanModelType(
    bool log_failed_eligibility_reason) {
  if (!is_feature_enabled_) {
    return is_server_model_enabled_ ? ModelType::kNotSupportedServerSide
                                    : ModelType::kNotSupportedOnDevice;
  }
  if (is_server_model_enabled_) {
    return !!remote_model_executor_ ? ModelType::kServerSide
                                    : ModelType::kNotSupportedServerSide;
  }
  if (log_failed_eligibility_reason && !on_device_model_available_) {
    LogOnDeviceModelEligibilityReason();
  }
  return on_device_model_available_ ? ModelType::kOnDevice
                                    : ModelType::kNotSupportedOnDevice;
}

bool ClientSideDetectionIntelligentScanDelegateDesktop::ShouldShowScamWarning(
    std::optional<IntelligentScanVerdict> verdict) {
  if (!verdict.has_value() ||
      *verdict ==
          IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_UNSPECIFIED ||
      *verdict == IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE ||
      *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_TELEMETRY) {
    return false;
  }

  return *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1 ||
         *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2 ||
         *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_3 ||
         *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_4 ||
         *verdict ==
             IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT;
}

void ClientSideDetectionIntelligentScanDelegateDesktop::OnPrefsUpdated() {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    return;
  }

  bool is_managed = management_service_ && management_service_->IsManaged();

  if (IsEnhancedProtectionEnabled(*pref_) && !is_managed) {
    if (!is_server_model_enabled_) {
      StartListeningToOnDeviceModelUpdate();
    }
  } else {
    StopListeningToOnDeviceModelUpdate();
  }
}

std::optional<base::UnguessableToken>
ClientSideDetectionIntelligentScanDelegateDesktop::StartIntelligentScan(
    std::string rendered_texts,
    IntelligentScanDoneCallback callback) {
  // We have checked the model availability prior to calling this function, but
  // we want to check one last time before creating a session.
  ModelType model_type =
      GetIntelligentScanModelType(/*log_failed_eligibility_reason=*/false);
  if (!IntelligentScanDelegate::IsIntelligentScanAvailable(model_type)) {
    std::move(callback).Run(IntelligentScanResult::Failure(
        IntelligentScanResult::kModelVersionUnavailable, model_type,
        is_server_model_enabled_
            ? IntelligentScanInfo::SERVER_SIDE_MODEL_UNAVAILABLE
            : IntelligentScanInfo::ON_DEVICE_MODEL_UNAVAILABLE));
    return std::nullopt;
  }

  bool is_at_quota = IsAtIntelligentScanQuota();
  if (is_server_model_enabled_) {
    // Only server model checks quota at inquiry time.
    base::UmaHistogramBoolean(
        "SBClientPhishing.ServerSideModelHitQuotaAtInquiryTime", is_at_quota);
  }
  if (is_at_quota) {
    std::move(callback).Run(IntelligentScanResult::Failure(
        IntelligentScanResult::kModelVersionUnavailable, ModelType::kServerSide,
        IntelligentScanInfo::SERVER_SIDE_MODEL_EXCEED_QUOTA));
    return std::nullopt;
  }

  base::UnguessableToken scan_id = base::UnguessableToken::Create();
  std::unique_ptr<Inquiry> new_inquiry =
      std::make_unique<Inquiry>(this, scan_id, std::move(callback));
  inquiries_[scan_id] = std::move(new_inquiry);
  inquiries_[scan_id]->Start(rendered_texts);
  return scan_id;
}

bool ClientSideDetectionIntelligentScanDelegateDesktop::CancelIntelligentScan(
    const base::UnguessableToken& scan_id) {
  if (!inquiries_.contains(scan_id)) {
    return false;
  }

  inquiries_.erase(scan_id);
  return true;
}

bool ClientSideDetectionIntelligentScanDelegateDesktop::ResetAllInquiries() {
  bool did_reset_inquiries = !inquiries_.empty();
  inquiries_.clear();
  return did_reset_inquiries;
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
  ResetAllInquiries();
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
  remote_model_executor_ = nullptr;
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
    client_side_detection::LogOnDeviceModelDownloadSuccess(false, reason);
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

void ClientSideDetectionIntelligentScanDelegateDesktop::OnScamWarningShown() {
  if (!is_server_model_enabled_) {
    return;
  }

  base::UmaHistogramCounts100(
      "SBClientPhishing.ServerSideModelQuotaCountOnScamWarningShown",
      pref_->GetList(prefs::kSafeBrowsingCsdIntelligentScanTimestamps).size());

  // The scan shows a warning and is effective, so we refund the quota.
  RemoveLastIntelligentScanQuota();
}

bool ClientSideDetectionIntelligentScanDelegateDesktop::
    IsAtIntelligentScanQuota() {
  if (!is_server_model_enabled_) {
    return false;
  }
  // Clear the expired timestamps
  ScopedListPrefUpdate update(pref_.get(),
                              prefs::kSafeBrowsingCsdIntelligentScanTimestamps);
  update->EraseIf([&](const base::Value& timestamp_value) {
    constexpr base::TimeDelta kIntelligentScanQuotaInterval = base::Days(1);
    std::optional<base::Time> report_time = base::ValueToTime(timestamp_value);
    if (!report_time.has_value()) {
      // If the value cannot be converted to a time, consider it invalid and
      // remove it.
      return true;
    }
    return *report_time + kIntelligentScanQuotaInterval < base::Time::Now();
  });
  return update->size() >=
         static_cast<size_t>(
             kClientSideDetectionServerModelMaxScansPerDayDesktop.Get());
}

void ClientSideDetectionIntelligentScanDelegateDesktop::
    AddIntelligentScanQuota() {
  ScopedListPrefUpdate update(pref_.get(),
                              prefs::kSafeBrowsingCsdIntelligentScanTimestamps);
  update->Append(base::TimeToValue(base::Time::Now()));
  base::UmaHistogramCounts100(
      "SBClientPhishing.ServerSideModelQuotaCountOnLookup", update->size());
}

void ClientSideDetectionIntelligentScanDelegateDesktop::
    RemoveLastIntelligentScanQuota() {
  ScopedListPrefUpdate update(pref_.get(),
                              prefs::kSafeBrowsingCsdIntelligentScanTimestamps);
  base::UmaHistogramBoolean(
      "SBClientPhishing.ServerSideModelPrefEmptyWhenRemovingQuota",
      update->empty());
  if (!update->empty()) {
    update->erase(update->end() - 1);
  }
}
}  // namespace safe_browsing
