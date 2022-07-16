// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include "base/base64.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/signals_type.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service.h"
#include "components/prefs/pref_service.h"

namespace enterprise_connectors {

using CollectSignalsCallback = SignalsService::CollectSignalsCallback;

namespace {

const base::ListValue& GetTrustedUrlPatterns(PrefService* prefs) {
  return *prefs->GetList(kContextAwareAccessSignalsAllowlistPref);
}

}  // namespace

// static
bool DeviceTrustService::IsEnabled(PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(kDeviceTrustConnectorEnabled)) {
    return false;
  }

  const auto& list = GetTrustedUrlPatterns(prefs);
  return !list.GetList().empty();
}

DeviceTrustService::DeviceTrustService(
    PrefService* profile_prefs,
    std::unique_ptr<AttestationService> attestation_service,
    std::unique_ptr<SignalsService> signals_service)
    : profile_prefs_(profile_prefs),
      attestation_service_(std::move(attestation_service)),
      signals_service_(std::move(signals_service)) {
  pref_observer_.Init(profile_prefs_);
  pref_observer_.Add(kContextAwareAccessSignalsAllowlistPref,
                     base::BindRepeating(&DeviceTrustService::OnPolicyUpdated,
                                         weak_factory_.GetWeakPtr()));
}

DeviceTrustService::DeviceTrustService(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {}

DeviceTrustService::~DeviceTrustService() {
  DCHECK(callbacks_.empty());
}

void DeviceTrustService::Shutdown() {
  pref_observer_.Remove(kContextAwareAccessSignalsAllowlistPref);
}

bool DeviceTrustService::IsEnabled() const {
  return IsEnabled(profile_prefs_);
}

void DeviceTrustService::OnPolicyUpdated() {
  if (IsEnabled() && !callbacks_.empty()) {
    callbacks_.Notify(GetTrustedUrlPatterns(profile_prefs_));
  }
}

void DeviceTrustService::BuildChallengeResponse(const std::string& challenge,
                                                AttestationCallback callback) {
  GetSignals(base::BindOnce(&DeviceTrustService::OnSignalsCollected,
                            weak_factory_.GetWeakPtr(), challenge,
                            std::move(callback)));
}

void DeviceTrustService::GetSignals(CollectSignalsCallback callback) {
  return signals_service_->CollectSignals(std::move(callback));
}

void DeviceTrustService::OnSignalsCollected(
    const std::string& challenge,
    AttestationCallback callback,
    std::unique_ptr<SignalsType> signals) {
  attestation_service_->BuildChallengeResponseForVAChallenge(
      challenge, std::move(signals), std::move(callback));
}

base::CallbackListSubscription
DeviceTrustService::RegisterTrustedUrlPatternsChangedCallback(
    TrustedUrlPatternsChangedCallback callback) {
  // Notify the callback right away so that caller can initialize itself.
  if (IsEnabled()) {
    callback.Run(GetTrustedUrlPatterns(profile_prefs_));
  }

  return callbacks_.Add(callback);
}

}  // namespace enterprise_connectors
