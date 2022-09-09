// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include "base/base64.h"
#include "base/values.h"
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

namespace {

// Runs the `callback` to return the `result` from the data decoder
// service after it is validated and decoded.
void OnJsonParsed(DeviceTrustService::ParseJsonChallengeCallback callback,
                  data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    std::move(callback).Run(std::string());
    return;
  }

  // Check if json is malformed or it doesn't include the needed field.
  const std::string* challenge = result->FindStringPath("challenge");
  if (!challenge) {
    std::move(callback).Run(std::string());
    return;
  }

  std::string serialized_signed_challenge;
  if (!base::Base64Decode(*challenge, &serialized_signed_challenge)) {
    std::move(callback).Run(std::string());
    return;
  }
  std::move(callback).Run(serialized_signed_challenge);
  return;
}

}  // namespace

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
  ParseJsonChallenge(
      challenge,
      base::BindOnce(&DeviceTrustService::OnChallengeParsed,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

bool DeviceTrustService::Watches(const GURL& url) const {
  return connector_ && connector_->Watches(url);
}

void DeviceTrustService::ParseJsonChallenge(
    const std::string& challenge,
    ParseJsonChallengeCallback callback) {
  data_decoder_.ParseJson(challenge,
                          base::BindOnce(&OnJsonParsed, std::move(callback)));
}

void DeviceTrustService::OnChallengeParsed(
    AttestationCallback callback,
    const std::string& serialized_signed_challenge) {
  GetSignals(base::BindOnce(&DeviceTrustService::OnSignalsCollected,
                            weak_factory_.GetWeakPtr(),
                            serialized_signed_challenge, std::move(callback)));
}

void DeviceTrustService::GetSignals(CollectSignalsCallback callback) {
  return signals_service_->CollectSignals(std::move(callback));
}

void DeviceTrustService::OnSignalsCollected(
    const std::string& serialized_signed_challenge,
    AttestationCallback callback,
    base::Value::Dict signals) {
  LogAttestationFunnelStep(DTAttestationFunnelStep::kSignalsCollected);

  attestation_service_->BuildChallengeResponseForVAChallenge(
      serialized_signed_challenge, std::move(signals), std::move(callback));
}

}  // namespace enterprise_connectors
