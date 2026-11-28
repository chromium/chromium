// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_subtle.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_with_timeout.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/enterprise/device_trust/core/attestation/attestation_utils.h"
#include "components/enterprise/device_trust/core/common_types.h"

namespace enterprise_connectors {

namespace {

using ash::attestation::TpmChallengeKeyResultCode;

DTAttestationResult ToAttestationResult(TpmChallengeKeyResultCode code) {
  // Map the error codes as best as possible to the DTAttestationResult. The
  // `kFailedToGenerateResponse` will be considered the bucket of all unmappable
  // errors.
  switch (code) {
    case TpmChallengeKeyResultCode::kKeyRegistrationFailedError:
    case TpmChallengeKeyResultCode::kUserKeyNotAvailableError:
      return DTAttestationResult::kMissingSigningKey;
    case TpmChallengeKeyResultCode::kChallengeBadBase64Error:
      return DTAttestationResult::kBadChallengeFormat;
    default:
      return DTAttestationResult::kFailedToGenerateResponse;
  }
}

// Returns the VerifiedAccessFlow which should be used for attestation depending
// on the current device context.
::attestation::VerifiedAccessFlow GetVerifiedAccessFlow() {
  if (ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    return ::attestation::ENTERPRISE_MACHINE;
  }

  return ::attestation::DEVICE_TRUST_CONNECTOR;
}

AshAttestationServiceImpl::Username GetUserNameForKeyName(Profile* profile) {
  if (!profile) {
    return AshAttestationServiceImpl::Username();
  }

  if (profile->IsEnterpriseIsolatedModeProfile()) {
    profile = profile->GetOriginalProfile();
  }

  return AshAttestationServiceImpl::Username(profile->GetProfileUserName());
}

// Returns the key name used for attestation. For ENTERPRISE_MACHINE the default
// key is used. Otherwise the key name is determined based on the username.
AshAttestationServiceImpl::KeyName GetKeyName(
    ::attestation::VerifiedAccessFlow flow_type,
    const AshAttestationServiceImpl::Username& username) {
  if (flow_type == ::attestation::ENTERPRISE_MACHINE) {
    return AshAttestationServiceImpl::KeyName();
  }
  return AshAttestationServiceImpl::GetDeviceTrustConnectorUserKeyName(
      username);
}

}  // namespace

AshAttestationServiceImpl::AshAttestationServiceImpl(Profile* profile)
    : profile_(profile) {}
AshAttestationServiceImpl::~AshAttestationServiceImpl() = default;

AshAttestationServiceImpl::KeyName
AshAttestationServiceImpl::GetDeviceTrustConnectorUserKeyName(
    const Username& username) {
  return KeyName(ash::attestation::kDeviceTrustConnectorKeyPrefix +
                 username.value());
}

base::WeakPtr<AshAttestationServiceImpl>
AshAttestationServiceImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AshAttestationServiceImpl::TryPrepareKey() {
  ::attestation::VerifiedAccessFlow flow_type = GetVerifiedAccessFlow();

  if (flow_type == ::attestation::ENTERPRISE_MACHINE) {
    // Enterprise machine keys don't need to be prepared since they are uploaded
    // to DMServer after enrollment.
    return;
  }

  KeyName key_name =
      AshAttestationServiceImpl::GetDeviceTrustConnectorUserKeyName(
          GetUserNameForKeyName(profile_));

  auto tpm_key_challenge_subtle =
      ash::attestation::TpmChallengeKeySubtleFactory::Create();
  auto* tpm_key_challenge_subtle_ptr = tpm_key_challenge_subtle.get();
  const uint64_t request_id = ++next_request_id_;
  pending_key_preparations_.emplace(request_id,
                                    std::move(tpm_key_challenge_subtle));
  tpm_key_challenge_subtle_ptr->StartPrepareKeyStep(
      flow_type, /*register_key=*/false,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name_for_spkac=*/key_name.value(), profile_,
      base::BindOnce(&AshAttestationServiceImpl::KeyPrepareCallback,
                     weak_factory_.GetWeakPtr(), request_id),
      std::nullopt);
}

void AshAttestationServiceImpl::KeyPrepareCallback(
    uint64_t request_id,
    const ash::attestation::TpmChallengeKeyResult& result) {
  if (!result.IsSuccess()) {
    LOG(ERROR) << "Key preparation failed with error: "
               << result.GetErrorMessage();
  }
  pending_key_preparations_.erase(request_id);
}

void AshAttestationServiceImpl::BuildChallengeResponseForVAChallenge(
    const std::string& serialized_signed_challenge,
    base::DictValue signals,
    const std::set<DTCPolicyLevel>& levels,
    AttestationCallback callback) {
  std::string signals_json;
  if (!base::JSONWriter::Write(signals, &signals_json)) {
    std::move(callback).Run(
        {std::string(), DTAttestationResult::kFailedToSerializeSignals});
    return;
  }

  ::attestation::VerifiedAccessFlow flow_type = GetVerifiedAccessFlow();
  KeyName key_name = GetKeyName(flow_type, GetUserNameForKeyName(profile_));

  // Using flow type DEVICE_TRUST_CONNECTOR for attestation is currently only
  // supported inside a session (including Enterprise Isolated Mode profiles).
  CHECK(flow_type != ::attestation::DEVICE_TRUST_CONNECTOR ||
        (profile_ && (profile_->IsRegularProfile() ||
                      profile_->IsEnterpriseIsolatedModeProfile())));

  auto tpm_key_challenger =
      std::make_unique<ash::attestation::TpmChallengeKeyWithTimeout>();
  auto* tpm_key_challenger_ptr = tpm_key_challenger.get();
  const uint64_t request_id = ++next_request_id_;
  pending_challenges_.emplace(request_id, std::move(tpm_key_challenger));
  tpm_key_challenger_ptr->BuildResponse(
      base::Seconds(15), flow_type, profile_,
      base::BindOnce(&AshAttestationServiceImpl::ReturnResult,
                     weak_factory_.GetWeakPtr(), request_id,
                     std::move(callback)),
      serialized_signed_challenge, /*register_key=*/false,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/key_name.value(),
      /*signals=*/signals_json);
}

void AshAttestationServiceImpl::ReturnResult(
    uint64_t request_id,
    AttestationCallback callback,
    const ash::attestation::TpmChallengeKeyResult& result) {
  std::string encoded_response;
  if (result.IsSuccess()) {
    encoded_response =
        ProtobufChallengeToJsonChallenge(result.challenge_response);
  } else {
    LOG(ERROR) << "Device Trust TPM error: " << result.GetErrorMessage();
  }
  const DTAttestationResult result_code =
      encoded_response.empty() ? ToAttestationResult(result.result_code)
                               : DTAttestationResult::kSuccess;
  pending_challenges_.erase(request_id);
  std::move(callback).Run({encoded_response, result_code});
}

}  // namespace enterprise_connectors
