// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include "base/base64.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/signals_type.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service.h"
#include "components/prefs/pref_service.h"

namespace enterprise_connectors {

using CollectSignalsCallback = SignalsService::CollectSignalsCallback;

DeviceTrustService::DeviceTrustService(
    std::unique_ptr<AttestationService> attestation_service,
    std::unique_ptr<SignalsService> signals_service,
    DeviceTrustConnectorService* connector)
    : attestation_service_(std::move(attestation_service)),
      signals_service_(std::move(signals_service)),
      connector_(connector) {
  DCHECK(attestation_service_);
  DCHECK(signals_service_);
  DCHECK(connector_);
}

DeviceTrustService::DeviceTrustService() = default;

DeviceTrustService::~DeviceTrustService() = default;

bool DeviceTrustService::IsEnabled() const {
  return connector_ && connector_->IsConnectorEnabled();
}

void DeviceTrustService::BuildChallengeResponse(const std::string& challenge,
                                                AttestationCallback callback) {
  GetSignals(base::BindOnce(&DeviceTrustService::OnSignalsCollected,
                            weak_factory_.GetWeakPtr(), challenge,
                            std::move(callback)));
}

bool DeviceTrustService::Watches(const GURL& url) const {
  return connector_ && connector_->Watches(url);
}

void DeviceTrustService::GetSignals(CollectSignalsCallback callback) {
  return signals_service_->CollectSignals(std::move(callback));
}

void DeviceTrustService::OnSignalsCollected(
    const std::string& challenge,
    AttestationCallback callback,
    std::unique_ptr<SignalsType> signals) {
  LogAttestationFunnelStep(DTAttestationFunnelStep::kSignalsCollected);

  attestation_service_->BuildChallengeResponseForVAChallenge(
      challenge, std::move(signals), std::move(callback));
}

}  // namespace enterprise_connectors
