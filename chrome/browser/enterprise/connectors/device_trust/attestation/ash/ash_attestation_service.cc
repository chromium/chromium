// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_service.h"

#include "base/base64.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/profiles/profile.h"

namespace enterprise_connectors {

AshAttestationService::AshAttestationService(Profile* profile)
    : profile_(profile) {}
AshAttestationService::~AshAttestationService() = default;

void AshAttestationService::BuildChallengeResponseForVAChallenge(
    const std::string& challenge,
    AttestationCallback callback) {
  tpm_key_challenger_ =
      std::make_unique<ash::attestation::TpmChallengeKeyWithTimeout>();
  tpm_key_challenger_->BuildResponse(
      base::TimeDelta::FromSeconds(15), ash::attestation::KEY_DEVICE, profile_,
      base::BindOnce(&AshAttestationService::ReturnResult,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      JsonChallengeToProtobufChallenge(challenge), /*register_key=*/false,
      /*key_name_for_spkac=*/"");
}

void AshAttestationService::ReturnResult(
    AttestationCallback callback,
    const ash::attestation::TpmChallengeKeyResult& result) {
  if (!result.IsSuccess())
    LOG(WARNING) << "Device attestation error: " << result.GetErrorMessage();

  std::string encoded_response;
  base::Base64Encode(result.challenge_response, &encoded_response);
  std::move(callback).Run(encoded_response);
}

}  // namespace enterprise_connectors
