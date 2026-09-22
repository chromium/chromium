// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/client_side_detection_intelligent_scan_delegate_android.h"

#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

namespace {
using ScamDetectionRequest = optimization_guide::proto::ScamDetectionRequest;
using ModelType = IntelligentScanDelegate::ModelType;

// The maximum number of intelligent scans that can be performed per day.
constexpr int kMaxIntelligentScansPerDay = 5;
}  // namespace

class ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry {
 public:
  Inquiry(ClientSideDetectionIntelligentScanDelegateAndroid* parent,
          const base::UnguessableToken& scan_id,
          IntelligentScanDoneCallback callback);
  ~Inquiry();

  void Start(const std::string& rendered_texts);

 private:
  void RemoteExecutionCallback(
      base::TimeTicks remote_execution_start_time,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // The parent object is guaranteed to outlive this object because the parent
  // owns this object.
  const raw_ptr<ClientSideDetectionIntelligentScanDelegateAndroid> parent_;
  base::UnguessableToken scan_id_;
  IntelligentScanDoneCallback callback_;
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
  // Server model does not return model version. Check the rollout feature flag
  // to set the model version.
  int model_version =
      base::FeatureList::IsEnabled(
          kClientSideDetectionServerModelRolloutAndroid)
          ? kClientSideDetectionServerModelRolloutVersionAndroid.Get()
          : IntelligentScanResult::kDefaultServerModelVersion;
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

ClientSideDetectionIntelligentScanDelegateAndroid::
    ClientSideDetectionIntelligentScanDelegateAndroid(
        PrefService& pref,
        optimization_guide::RemoteModelExecutor* remote_model_executor)
    : pref_(pref),
      remote_model_executor_(remote_model_executor),
      is_feature_enabled_(
          !base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
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
  return verdict->client_side_detection_type() ==
             ClientSideDetectionType::FORCE_REQUEST &&
         verdict->has_llama_forced_trigger_info() &&
         verdict->llama_forced_trigger_info().intelligent_scan();
}

ModelType
ClientSideDetectionIntelligentScanDelegateAndroid::GetIntelligentScanModelType(
    bool log_failed_eligibility_reason) {
  if (!is_feature_enabled_ || !remote_model_executor_) {
    return ModelType::kNotSupportedServerSide;
  }
  return ModelType::kServerSide;
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
        IntelligentScanInfo::SERVER_SIDE_MODEL_UNAVAILABLE));
    return std::nullopt;
  }
  bool is_at_quota = IsAtIntelligentScanQuota();
  base::UmaHistogramBoolean(
      "SBClientPhishing.ServerSideModelHitQuotaAtInquiryTime", is_at_quota);
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

void ClientSideDetectionIntelligentScanDelegateAndroid::OnScamWarningShown() {
  base::UmaHistogramCounts100(
      "SBClientPhishing.ServerSideModelQuotaCountOnScamWarningShown",
      pref_->GetList(prefs::kSafeBrowsingCsdIntelligentScanTimestamps).size());

  // The scan shows a warning and is effective, so we refund the quota.
  RemoveLastIntelligentScanQuota();
}

void ClientSideDetectionIntelligentScanDelegateAndroid::Shutdown() {
  ResetAllInquiries();
  remote_model_executor_ = nullptr;
  pref_change_registrar_.RemoveAll();
}

void ClientSideDetectionIntelligentScanDelegateAndroid::OnPrefsUpdated() {
  if (!is_feature_enabled_) {
    return;
  }
  if (!IsEnhancedProtectionEnabled(*pref_)) {
    ResetAllInquiries();
  }
}

bool ClientSideDetectionIntelligentScanDelegateAndroid::
    IsAtIntelligentScanQuota() {
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
  return update->size() >= static_cast<size_t>(kMaxIntelligentScansPerDay);
}

void ClientSideDetectionIntelligentScanDelegateAndroid::
    AddIntelligentScanQuota() {
  ScopedListPrefUpdate update(pref_.get(),
                              prefs::kSafeBrowsingCsdIntelligentScanTimestamps);
  update->Append(base::TimeToValue(base::Time::Now()));
  base::UmaHistogramCounts100(
      "SBClientPhishing.ServerSideModelQuotaCountOnLookup", update->size());
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
