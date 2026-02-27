// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/client_side_detection_intelligent_scan_delegate_android.h"

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_util.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"

namespace safe_browsing {

namespace {
using optimization_guide::mojom::OnDeviceFeature::kScamDetection;
using ScamDetectionRequest = optimization_guide::proto::ScamDetectionRequest;
using ModelType = IntelligentScanDelegate::ModelType;
}  // namespace

class ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry {
 public:
  Inquiry(ClientSideDetectionIntelligentScanDelegateAndroid* parent,
          const base::UnguessableToken& scan_id,
          IntelligentScanDoneCallback callback);
  ~Inquiry();

  void Start(const std::string& rendered_texts);

 private:
  using ModelExecutorSession = optimization_guide::OnDeviceSession;

  void OnSessionCreated(base::TimeTicks session_creation_start_time,
                        std::unique_ptr<ModelExecutorSession> session);

  void ModelExecutionCallback(
      base::TimeTicks session_execution_start_time,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  void RemoteExecutionCallback(
      base::TimeTicks remote_execution_start_time,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // The parent object is guaranteed to outlive this object because the parent
  // owns this object.
  const raw_ptr<ClientSideDetectionIntelligentScanDelegateAndroid> parent_;
  std::unique_ptr<ModelExecutorSession> session_;
  base::UnguessableToken scan_id_;
  IntelligentScanDoneCallback callback_;
  std::string rendered_texts_;
  bool was_start_called_ = false;

  base::WeakPtrFactory<Inquiry> weak_factory_{this};
};

ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry::Inquiry(
    ClientSideDetectionIntelligentScanDelegateAndroid* parent,
    const base::UnguessableToken& scan_id,
    IntelligentScanDoneCallback callback)
    : parent_(parent), scan_id_(scan_id), callback_(std::move(callback)) {}

ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry::~Inquiry() =
    default;

void ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry::Start(
    const std::string& rendered_texts) {
  CHECK(!was_start_called_)
      << "Start() should only be called once per inquiry.";
  was_start_called_ = true;

  if (parent_->is_server_model_enabled_) {
    parent_->AddIntelligentScanQuota();
    ScamDetectionRequest request;
    request.set_rendered_text(rendered_texts);
    parent_->remote_model_executor_->ExecuteModel(
        optimization_guide::ModelBasedCapabilityKey::kScamDetection,
        std::move(request), /*options=*/{},
        base::BindOnce(&ClientSideDetectionIntelligentScanDelegateAndroid::
                           Inquiry::RemoteExecutionCallback,
                       weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
    // Do not access `parent_` at this point. The callback may be called
    // immediately and this object will delete itself.
    return;
  }

  rendered_texts_ = rendered_texts;
  parent_->model_broker_client_->CreateSession(
      kScamDetection, ::optimization_guide::SessionConfigParams{},
      base::BindOnce(&ClientSideDetectionIntelligentScanDelegateAndroid::
                         Inquiry::OnSessionCreated,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry::
    OnSessionCreated(base::TimeTicks session_creation_start_time,
                     std::unique_ptr<ModelExecutorSession> session) {
  bool is_model_available = session != nullptr;
  base::UmaHistogramBoolean(
      "SBClientPhishing.IsOnDeviceModelAvailableOnSessionCreation",
      is_model_available);
  if (!is_model_available) {
    std::move(callback_).Run(IntelligentScanResult::Failure(
        IntelligentScanResult::kModelVersionUnavailable,
        ModelType::kNotSupportedOnDevice,
        IntelligentScanInfo::ON_DEVICE_MODEL_UNAVAILABLE));
    return;
  }
  client_side_detection::LogOnDeviceModelSessionCreationTime(
      session_creation_start_time);
  session_ = std::move(session);

  if (parent_->pause_inquiry_for_testing_) {
    return;
  }

  ScamDetectionRequest request;
  request.set_rendered_text(rendered_texts_);

  session_->ExecuteModel(
      *std::make_unique<ScamDetectionRequest>(request),
      base::BindRepeating(&ClientSideDetectionIntelligentScanDelegateAndroid::
                              Inquiry::ModelExecutionCallback,
                          weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry::
    ModelExecutionCallback(
        base::TimeTicks session_execution_start_time,
        optimization_guide::OptimizationGuideModelStreamingExecutionResult
            result) {
  CHECK(callback_);
  int model_version = IntelligentScanResult::kModelVersionUnavailable;
  if (result.execution_info) {
    model_version = result.execution_info->on_device_model_execution_info()
                        .model_versions()
                        .on_device_model_service_version()
                        .model_adaptation_version();
  }

  if (!result.response.has_value()) {
    client_side_detection::LogOnDeviceModelExecutionSuccessAndTime(
        /*success=*/false, session_execution_start_time);
    std::move(callback_).Run(IntelligentScanResult::Failure(
        model_version, ModelType::kOnDevice,
        IntelligentScanInfo::ON_DEVICE_MODEL_OUTPUT_MISSING));
    return;
  }

  // This is a non-error response, but it's not completed, yet so we wait till
  // it's complete. We will not respond to the callback yet because of this.
  if (!result.response->is_complete) {
    return;
  }

  client_side_detection::LogOnDeviceModelExecutionSuccessAndTime(
      /*success=*/true, session_execution_start_time);

  auto scam_detection_response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ScamDetectionResponse>(
      result.response->response);

  if (!scam_detection_response) {
    base::debug::DumpWithoutCrashing();
    std::move(callback_).Run(IntelligentScanResult::Failure(
        model_version, ModelType::kOnDevice,
        IntelligentScanInfo::ON_DEVICE_MODEL_OUTPUT_MISSING));
    return;
  }

  std::move(callback_).Run(IntelligentScanResult::Success(
      scam_detection_response->brand(), scam_detection_response->intent(),
      model_version, ModelType::kOnDevice));

  // Reset this inquiry immediately so that future inference is not affected by
  // the old context.
  parent_->CancelIntelligentScan(scan_id_);
}

void ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry::
    RemoteExecutionCallback(
        base::TimeTicks remote_execution_start_time,
        optimization_guide::OptimizationGuideModelExecutionResult result,
        std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  CHECK(callback_);
  bool execution_success = result.response.has_value();
  base::UmaHistogramBoolean("SBClientPhishing.ServerSideModelExecutionSuccess",
                            execution_success);
  base::UmaHistogramMediumTimes(
      "SBClientPhishing.ServerSideModelExecutionDuration",
      base::TimeTicks::Now() - remote_execution_start_time);
  // Server model does not return model version.
  int model_version = IntelligentScanResult::kModelVersionUnavailable;
  if (!execution_success) {
    base::UmaHistogramEnumeration(
        "SBClientPhishing.ServerSideModelExecutionError",
        result.response.error().error());
    std::move(callback_).Run(IntelligentScanResult::Failure(
        model_version, ModelType::kServerSide,
        IntelligentScanInfo::SERVER_SIDE_MODEL_OUTPUT_MISSING));
    return;
  }

  auto scam_detection_response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ScamDetectionResponse>(
      result.response.value());

  if (!scam_detection_response) {
    std::move(callback_).Run(IntelligentScanResult::Failure(
        model_version, ModelType::kServerSide,
        IntelligentScanInfo::SERVER_SIDE_MODEL_OUTPUT_MISSING));
    return;
  }
  std::move(callback_).Run(IntelligentScanResult::Success(
      scam_detection_response->brand(), scam_detection_response->intent(),
      model_version, ModelType::kServerSide));

  // Reset this inquiry immediately so that future inference is not affected by
  // the old context.
  parent_->CancelIntelligentScan(scan_id_);
}

ClientSideDetectionIntelligentScanDelegateAndroid::
    ClientSideDetectionIntelligentScanDelegateAndroid(
        PrefService& pref,
        std::unique_ptr<optimization_guide::ModelBrokerClient>
            model_broker_client,
        optimization_guide::RemoteModelExecutor* remote_model_executor)
    : pref_(pref),
      model_broker_client_(std::move(model_broker_client)),
      remote_model_executor_(remote_model_executor),
      is_feature_enabled_(
          !base::FeatureList::IsEnabled(kClientSideDetectionKillswitch) &&
          (base::FeatureList::IsEnabled(
               kClientSideDetectionSendIntelligentScanInfoAndroid) ||
           kCsdImageEmbeddingMatchWithIntelligentScan.Get() ||
           base::FeatureList::IsEnabled(
               kClientSideDetectionServerModelForScamDetectionAndroid))),
      is_server_model_enabled_(base::FeatureList::IsEnabled(
          kClientSideDetectionServerModelForScamDetectionAndroid)) {
  if (!is_feature_enabled_) {
    return;
  }
  pref_change_registrar_.Init(&pref);
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(
          &ClientSideDetectionIntelligentScanDelegateAndroid::OnPrefsUpdated,
          base::Unretained(this)));
  //  Do an initial check of the prefs.
  OnPrefsUpdated();
}

ClientSideDetectionIntelligentScanDelegateAndroid::
    ~ClientSideDetectionIntelligentScanDelegateAndroid() = default;

bool ClientSideDetectionIntelligentScanDelegateAndroid::
    ShouldRequestIntelligentScan(ClientPhishingRequest* verdict) {
  if (!is_feature_enabled_) {
    return false;
  }
  if (!IsEnhancedProtectionEnabled(*pref_)) {
    return false;
  }
  if (verdict->client_side_detection_type() ==
          ClientSideDetectionType::IMAGE_EMBEDDING_MATCH &&
      verdict->is_phishing() &&
      kCsdImageEmbeddingMatchWithIntelligentScan.Get()) {
    return true;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kScamDetectionKeyboardLockTriggerAndroid) &&
      verdict->client_side_detection_type() ==
          ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED) {
    return true;
  }
  return verdict->client_side_detection_type() ==
             ClientSideDetectionType::FORCE_REQUEST &&
         verdict->has_llama_forced_trigger_info() &&
         verdict->llama_forced_trigger_info().intelligent_scan();
}

ModelType
ClientSideDetectionIntelligentScanDelegateAndroid::GetIntelligentScanModelType(
    bool log_failed_eligibility_reason) {
  if (!is_feature_enabled_) {
    return is_server_model_enabled_ ? ModelType::kNotSupportedServerSide
                                    : ModelType::kNotSupportedOnDevice;
  }
  if (is_server_model_enabled_) {
    return !!remote_model_executor_ ? ModelType::kServerSide
                                    : ModelType::kNotSupportedServerSide;
  }
  if (!model_broker_client_) {
    return ModelType::kNotSupportedOnDevice;
  }
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionOnDeviceModelLazyDownloadAndroid)) {
    // When the lazy download flag is enabled, we will check model availability
    // at inquiry time.
    return ModelType::kOnDevice;
  }
  // The HasSubscriber check is required because GetSubscriber will start model
  // download if this is the first time the subscriber is requested.
  if (!model_broker_client_->HasSubscriber(kScamDetection)) {
    return ModelType::kNotSupportedOnDevice;
  }

  auto reason =
      model_broker_client_->GetSubscriber(kScamDetection).unavailable_reason();
  if (reason.has_value()) {
    if (log_failed_eligibility_reason) {
      base::UmaHistogramEnumeration(
          "SBClientPhishing.OnDeviceModelUnavailableReasonAtInquiry.Android",
          reason.value());
    }
    return ModelType::kNotSupportedOnDevice;
  }

  return ModelType::kOnDevice;
}

std::optional<base::UnguessableToken>
ClientSideDetectionIntelligentScanDelegateAndroid::StartIntelligentScan(
    std::string rendered_texts,
    IntelligentScanDoneCallback callback) {
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

bool ClientSideDetectionIntelligentScanDelegateAndroid::CancelIntelligentScan(
    const base::UnguessableToken& scan_id) {
  if (!inquiries_.contains(scan_id)) {
    return false;
  }
  inquiries_.erase(scan_id);
  return true;
}

bool ClientSideDetectionIntelligentScanDelegateAndroid::ResetAllInquiries() {
  bool did_reset_inquiry = !inquiries_.empty();
  inquiries_.clear();
  return did_reset_inquiry;
}

bool ClientSideDetectionIntelligentScanDelegateAndroid::ShouldShowScamWarning(
    std::optional<IntelligentScanVerdict> verdict) {
  if (!verdict.has_value() ||
      *verdict ==
          IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_UNSPECIFIED ||
      *verdict == IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionShowScamVerdictWarningAndroid) &&
      !base::FeatureList::IsEnabled(
          kClientSideDetectionServerModelForScamDetectionAndroid)) {
    return false;
  }

  return *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1 ||
         *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2 ||
         *verdict ==
             IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT;
}

void ClientSideDetectionIntelligentScanDelegateAndroid::OnScamWarningShown() {
  if (!is_server_model_enabled_) {
    return;
  }

  base::UmaHistogramCounts100(
      "SBClientPhishing.ServerSideModelQuotaCountOnScamWarningShown",
      pref_->GetList(prefs::kSafeBrowsingCsdIntelligentScanTimestamps).size());

  // The scan shows a warning and is effective, so we refund the quota.
  RemoveLastIntelligentScanQuota();
}

void ClientSideDetectionIntelligentScanDelegateAndroid::Shutdown() {
  client_side_detection::LogOnDeviceModelSessionAliveOnDelegateShutdown(
      !inquiries_.empty());
  ResetAllInquiries();
  model_broker_client_.reset();
  remote_model_executor_ = nullptr;
  pref_change_registrar_.RemoveAll();
}

void ClientSideDetectionIntelligentScanDelegateAndroid::OnPrefsUpdated() {
  if (!is_feature_enabled_) {
    return;
  }
  if (!IsEnhancedProtectionEnabled(*pref_)) {
    ResetAllInquiries();
    return;
  }
  // No need to download the on-device model at startup if we are using the
  // server model or the lazy download flag is enabled.
  if (!is_server_model_enabled_ &&
      !base::FeatureList::IsEnabled(
          kClientSideDetectionOnDeviceModelLazyDownloadAndroid)) {
    StartModelDownload();
  }
}

void ClientSideDetectionIntelligentScanDelegateAndroid::StartModelDownload() {
  if (!model_broker_client_) {
    return;
  }
  base::ScopedUmaHistogramTimer scoped_timer(
      "SBClientPhishing.OnDeviceModelStartModelDownloadFunctionRunTime."
      "Android");
  model_broker_client_->RequestAssetsFor(kScamDetection);
  model_broker_client_->GetSubscriber(kScamDetection)
      .WaitForClient(base::BindOnce(
          [](base::TimeTicks download_start_time,
             base::WeakPtr<optimization_guide::ModelClient> model_client) {
            client_side_detection::LogOnDeviceModelDownloadSuccess(
                !!model_client);
            if (model_client) {
              client_side_detection::LogOnDeviceModelFetchTime(
                  download_start_time);
            }
          },
          base::TimeTicks::Now()));
}

bool ClientSideDetectionIntelligentScanDelegateAndroid::
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
             kClientSideDetectionServerModelMaxScansPerDay.Get());
}

void ClientSideDetectionIntelligentScanDelegateAndroid::
    AddIntelligentScanQuota() {
  ScopedListPrefUpdate update(pref_.get(),
                              prefs::kSafeBrowsingCsdIntelligentScanTimestamps);
  update->Append(base::TimeToValue(base::Time::Now()));
}

void ClientSideDetectionIntelligentScanDelegateAndroid::
    RemoveLastIntelligentScanQuota() {
  ScopedListPrefUpdate update(pref_.get(),
                              prefs::kSafeBrowsingCsdIntelligentScanTimestamps);
  base::UmaHistogramBoolean(
      "SBClientPhishing.ServerSideModelPrefEmptyWhenRemovingQuota",
      update->empty());
  if (!update->empty()) {
    update->erase(update.Get().end() - 1);
  }
}

}  // namespace safe_browsing
