// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/client_side_detection_intelligent_scan_delegate_android.h"

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/safe_browsing/client_side_detection_intelligent_scan_delegate_util.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"

namespace safe_browsing {

namespace {
using optimization_guide::mojom::OnDeviceFeature::kScamDetection;
}  // namespace

class ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry {
 public:
  Inquiry(ClientSideDetectionIntelligentScanDelegateAndroid* parent,
          const base::UnguessableToken& session_id,
          InquireOnDeviceModelDoneCallback callback);
  ~Inquiry();

  void Start(const std::string& rendered_texts);

 private:
  using ModelExecutorSession = optimization_guide::OnDeviceSession;

  void OnSessionCreated(std::unique_ptr<ModelExecutorSession> session);

  void ModelExecutionCallback(
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  // The parent object is guaranteed to outlive this object because the parent
  // owns this object.
  const raw_ptr<ClientSideDetectionIntelligentScanDelegateAndroid> parent_;
  std::unique_ptr<ModelExecutorSession> session_;
  base::UnguessableToken session_id_;
  InquireOnDeviceModelDoneCallback callback_;
  std::string rendered_texts_;
  base::TimeTicks session_creation_start_time_;
  base::TimeTicks session_execution_start_time_;

  base::WeakPtrFactory<Inquiry> weak_factory_{this};
};

ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry::Inquiry(
    ClientSideDetectionIntelligentScanDelegateAndroid* parent,
    const base::UnguessableToken& session_id,
    InquireOnDeviceModelDoneCallback callback)
    : parent_(parent),
      session_id_(session_id),
      callback_(std::move(callback)) {}

ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry::~Inquiry() =
    default;

void ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry::Start(
    const std::string& rendered_texts) {
  CHECK(!session_) << "Start() should only be called once per inquiry.";

  rendered_texts_ = rendered_texts;
  session_creation_start_time_ = base::TimeTicks::Now();
  parent_->model_broker_client_->CreateSession(
      kScamDetection, ::optimization_guide::SessionConfigParams{},
      base::BindOnce(&ClientSideDetectionIntelligentScanDelegateAndroid::
                         Inquiry::OnSessionCreated,
                     weak_factory_.GetWeakPtr()));
}

void ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry::
    OnSessionCreated(std::unique_ptr<ModelExecutorSession> session) {
  CHECK(session) << "model broker client should not create a null session.";
  client_side_detection::LogOnDeviceModelSessionCreationTime(
      session_creation_start_time_);
  session_ = std::move(session);

  if (parent_->pause_session_execution_for_testing_) {
    return;
  }

  using ScamDetectionRequest = optimization_guide::proto::ScamDetectionRequest;
  ScamDetectionRequest request;
  request.set_rendered_text(rendered_texts_);

  session_execution_start_time_ = base::TimeTicks::Now();
  session_->ExecuteModel(
      *std::make_unique<ScamDetectionRequest>(request),
      base::BindRepeating(&ClientSideDetectionIntelligentScanDelegateAndroid::
                              Inquiry::ModelExecutionCallback,
                          weak_factory_.GetWeakPtr()));
}

void ClientSideDetectionIntelligentScanDelegateAndroid::Inquiry::
    ModelExecutionCallback(
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
        /*success=*/false, session_execution_start_time_);
    std::move(callback_).Run(IntelligentScanResult::Failure(model_version));
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
    base::debug::DumpWithoutCrashing();
    std::move(callback_).Run(IntelligentScanResult::Failure(model_version));
    return;
  }

  std::move(callback_).Run({.brand = scam_detection_response->brand(),
                            .intent = scam_detection_response->intent(),
                            .model_version = model_version,
                            .execution_success = true});

  // Reset session immediately so that future inference is not affected by the
  // old context.
  parent_->CancelSession(session_id_);
}

ClientSideDetectionIntelligentScanDelegateAndroid::
    ClientSideDetectionIntelligentScanDelegateAndroid(
        PrefService& pref,
        std::unique_ptr<optimization_guide::ModelBrokerClient>
            model_broker_client)
    : pref_(pref), model_broker_client_(std::move(model_broker_client)) {
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
  if (!IsEnhancedProtectionEnabled(*pref_)) {
    return false;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kScamDetectionKeyboardLockTriggerAndroid) &&
      verdict->client_side_detection_type() ==
          ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED) {
    return true;
  }
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionSendIntelligentScanInfoAndroid)) {
    return false;
  }
  return verdict->client_side_detection_type() ==
             ClientSideDetectionType::FORCE_REQUEST &&
         verdict->has_llama_forced_trigger_info() &&
         verdict->llama_forced_trigger_info().intelligent_scan();
}

bool ClientSideDetectionIntelligentScanDelegateAndroid::
    IsOnDeviceModelAvailable(bool log_failed_eligibility_reason) {
  if (!model_broker_client_) {
    return false;
  }
  // The HasSubscriber check is required because GetSubscriber may start model
  // download.
  if (!model_broker_client_->HasSubscriber(kScamDetection)) {
    return false;
  }

  auto reason =
      model_broker_client_->GetSubscriber(kScamDetection).unavailable_reason();
  if (reason.has_value()) {
    if (log_failed_eligibility_reason) {
      base::UmaHistogramEnumeration(
          "SBClientPhishing.OnDeviceModelUnavailableReasonAtInquiry.Android",
          reason.value());
    }
    return false;
  }

  return true;
}

std::optional<base::UnguessableToken>
ClientSideDetectionIntelligentScanDelegateAndroid::InquireOnDeviceModel(
    std::string rendered_texts,
    InquireOnDeviceModelDoneCallback callback) {
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

bool ClientSideDetectionIntelligentScanDelegateAndroid::CancelSession(
    const base::UnguessableToken& session_id) {
  if (!inquiries_.contains(session_id)) {
    return false;
  }
  inquiries_.erase(session_id);
  return true;
}

bool ClientSideDetectionIntelligentScanDelegateAndroid::ResetAllSessions() {
  bool did_reset_session = !inquiries_.empty();
  inquiries_.clear();
  return did_reset_session;
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
          kClientSideDetectionShowScamVerdictWarningAndroid)) {
    return false;
  }

  return *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1 ||
         *verdict == IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2 ||
         *verdict ==
             IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT;
}

void ClientSideDetectionIntelligentScanDelegateAndroid::Shutdown() {
  client_side_detection::LogOnDeviceModelSessionAliveOnDelegateShutdown(
      !inquiries_.empty());
  ResetAllSessions();
  model_broker_client_.reset();
  pref_change_registrar_.RemoveAll();
}

void ClientSideDetectionIntelligentScanDelegateAndroid::OnPrefsUpdated() {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    return;
  }
  bool is_feature_enabled = base::FeatureList::IsEnabled(
      kClientSideDetectionSendIntelligentScanInfoAndroid);
  if (IsEnhancedProtectionEnabled(*pref_) && is_feature_enabled) {
    StartModelDownload();
  } else {
    ResetAllSessions();
  }
}

void ClientSideDetectionIntelligentScanDelegateAndroid::StartModelDownload() {
  if (!model_broker_client_) {
    return;
  }
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

}  // namespace safe_browsing
