// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include "base/base64.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/signals_type.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
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
  const std::string* challenge = result->GetDict().FindString("challenge");
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

DeviceTrustResponse CreateFailedResponse(DeviceTrustError error) {
  DeviceTrustResponse response;
  response.error = error;
  return response;
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

void DeviceTrustService::BuildChallengeResponse(
    const std::string& serialized_challenge,
    const std::set<DTCPolicyLevel>& levels,
    DeviceTrustCallback callback) {
  ParseJsonChallenge(
      serialized_challenge,
      base::BindOnce(&DeviceTrustService::OnChallengeParsed,
                     weak_factory_.GetWeakPtr(), levels, std::move(callback)));
}

const std::set<DTCPolicyLevel> DeviceTrustService::Watches(
    const GURL& url) const {
  return connector_ ? connector_->Watches(url) : std::set<DTCPolicyLevel>();
}

void DeviceTrustService::ParseJsonChallenge(
    const std::string& serialized_challenge,
    ParseJsonChallengeCallback callback) {
  data_decoder_.ParseJson(serialized_challenge,
                          base::BindOnce(&OnJsonParsed, std::move(callback)));
}

void DeviceTrustService::OnChallengeParsed(
    const std::set<DTCPolicyLevel>& levels,
    DeviceTrustCallback callback,
    const std::string& challenge) {
  if (challenge.empty()) {
    // Failed to parse the challenge, fail early.
    std::move(callback).Run(
        CreateFailedResponse(DeviceTrustError::kFailedToParseChallenge));
    return;
  }

  GetSignals(base::BindOnce(&DeviceTrustService::OnSignalsCollected,
                            weak_factory_.GetWeakPtr(), challenge, levels,
                            std::move(callback)));
}

void DeviceTrustService::GetSignals(CollectSignalsCallback callback) {
  return signals_service_->CollectSignals(std::move(callback));
}

void DeviceTrustService::OnSignalsCollected(
    const std::string& challenge,
    const std::set<DTCPolicyLevel>& levels,
    DeviceTrustCallback callback,
    base::Value::Dict signals) {
  LogAttestationFunnelStep(DTAttestationFunnelStep::kSignalsCollected);

  attestation_service_->BuildChallengeResponseForVAChallenge(
      challenge, std::move(signals), levels,
      base::BindOnce(&DeviceTrustService::OnAttestationResponseReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DeviceTrustService::OnAttestationResponseReceived(
    DeviceTrustCallback callback,
    const AttestationResponse& attestation_response) {
  LogAttestationResult(attestation_response.result_code);

  DeviceTrustResponse dt_response{};
  dt_response.challenge_response = attestation_response.challenge_response;
  dt_response.attestation_result = attestation_response.result_code;

  if (!IsSuccessAttestationResult(attestation_response.result_code)) {
    dt_response.error = DeviceTrustError::kFailedToCreateResponse;
  }

  std::move(callback).Run(dt_response);
}

}  // namespace enterprise_connectors
