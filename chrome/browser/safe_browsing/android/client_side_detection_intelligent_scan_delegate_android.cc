// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/client_side_detection_intelligent_scan_delegate_android.h"

#include "base/notimplemented.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

namespace {
using optimization_guide::mojom::ModelBasedCapabilityKey::kScamDetection;
}  // namespace

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
  // TODO(crbug.com/424075615): Add UMA logging for failed eligibility reasons.
  // The HasSubscriber check is required because GetSubscriber may start model
  // download.
  return model_broker_client_->HasSubscriber(kScamDetection) &&
         !model_broker_client_->GetSubscriber(kScamDetection)
              .unavailable_reason()
              .has_value();
}

void ClientSideDetectionIntelligentScanDelegateAndroid::InquireOnDeviceModel(
    std::string rendered_texts,
    InquireOnDeviceModelDoneCallback callback) {
  NOTIMPLEMENTED();
  return;
}

bool ClientSideDetectionIntelligentScanDelegateAndroid::ResetOnDeviceSession() {
  return false;
}

bool ClientSideDetectionIntelligentScanDelegateAndroid::ShouldShowScamWarning(
    std::optional<IntelligentScanVerdict> verdict) {
  return false;
}

void ClientSideDetectionIntelligentScanDelegateAndroid::Shutdown() {
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
  }
}

void ClientSideDetectionIntelligentScanDelegateAndroid::StartModelDownload() {
  if (!model_broker_client_) {
    return;
  }
  model_broker_client_->GetSubscriber(kScamDetection);
}

}  // namespace safe_browsing
