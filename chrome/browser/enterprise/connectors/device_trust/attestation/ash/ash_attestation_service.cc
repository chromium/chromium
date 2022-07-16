// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_with_timeout.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"

namespace enterprise_connectors {

AshAttestationService::AshAttestationService(Profile* profile)
    : profile_(profile) {}
AshAttestationService::~AshAttestationService() = default;

void AshAttestationService::BuildChallengeResponseForVAChallenge(
    const std::string& challenge,
    std::unique_ptr<attestation::DeviceTrustSignals> signals,
    AttestationCallback callback) {
  DCHECK(signals);
  tpm_key_challenger_ =
      std::make_unique<ash::attestation::TpmChallengeKeyWithTimeout>();
  tpm_key_challenger_->BuildResponse(
      base::Seconds(15), ash::attestation::KEY_DEVICE, profile_,
      base::BindOnce(&AshAttestationService::ReturnResult,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      JsonChallengeToProtobufChallenge(challenge), /*register_key=*/false,
      /*key_name_for_spkac=*/std::string(), /*signals=*/*signals);
}

void AshAttestationService::ReturnResult(
    AttestationCallback callback,
    const ash::attestation::TpmChallengeKeyResult& result) {
  std::string encoded_response;
  if (result.IsSuccess()) {
    // TODO(crbug.com/1241405): Handle failure case better.
    base::Base64Encode(result.challenge_response, &encoded_response);
  }
  std::move(callback).Run(encoded_response);
}

}  // namespace enterprise_connectors
