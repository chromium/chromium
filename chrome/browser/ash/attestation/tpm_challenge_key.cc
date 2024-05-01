// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/tpm_challenge_key.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_subtle.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"

class Profile;
class AttestationFlow;

namespace ash {
namespace attestation {

//========================= TpmChallengeKeyFactory =============================

TpmChallengeKey* TpmChallengeKeyFactory::next_result_for_testing_ = nullptr;

// static
std::unique_ptr<TpmChallengeKey> TpmChallengeKeyFactory::Create() {
  if (next_result_for_testing_) [[unlikely]] {
    std::unique_ptr<TpmChallengeKey> result(next_result_for_testing_);
    next_result_for_testing_ = nullptr;
    return result;
  }

  return std::make_unique<TpmChallengeKeyImpl>();
}

// static
void TpmChallengeKeyFactory::SetForTesting(
    std::unique_ptr<TpmChallengeKey> next_result) {
  // unique_ptr itself cannot be stored in a static variable because of its
  // complex destructor.
  next_result_for_testing_ = next_result.release();
}

//=========================== TpmChallengeKeyImpl ==============================

TpmChallengeKeyImpl::TpmChallengeKeyImpl() {
  tpm_challenge_key_subtle_ = TpmChallengeKeySubtleFactory::Create();
}

TpmChallengeKeyImpl::TpmChallengeKeyImpl(
    AttestationFlow* attestation_flow_for_testing,
    MachineCertificateUploader* certificate_uploader_for_testing) {
  tpm_challenge_key_subtle_ = std::make_unique<TpmChallengeKeySubtleImpl>(
      attestation_flow_for_testing, certificate_uploader_for_testing);
}

TpmChallengeKeyImpl::~TpmChallengeKeyImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TpmChallengeKeyImpl::BuildResponse(
    ::attestation::VerifiedAccessFlow flow_type,
    Profile* profile,
    TpmChallengeKeyCallback callback,
    const std::string& challenge,
    bool register_key,
    ::attestation::KeyType key_crypto_type,
    const std::string& key_name,
    const std::optional<std::string>& signals) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());

  // For device key: if |register_key| is true, |key_name| should not be empty.
  DCHECK((flow_type != ::attestation::ENTERPRISE_MACHINE) ||
         (register_key == !key_name.empty()))
      << "Invalid arguments: " << register_key << " " << !key_name.empty();

  register_key_ = register_key;
  challenge_ = challenge;
  callback_ = std::move(callback);

  // Empty |key_name| means that some default name will be used.
  tpm_challenge_key_subtle_->StartPrepareKeyStep(
      flow_type, /*will_register_key=*/register_key_, key_crypto_type, key_name,
      profile,
      base::BindOnce(&TpmChallengeKeyImpl::OnPrepareKeyDone,
                     weak_factory_.GetWeakPtr()),
      signals);
}

void TpmChallengeKeyImpl::OnPrepareKeyDone(
    const TpmChallengeKeyResult& prepare_key_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!prepare_key_result.IsSuccess()) {
    std::move(callback_).Run(prepare_key_result);
    return;
  }

  tpm_challenge_key_subtle_->StartSignChallengeStep(
      challenge_, base::BindOnce(&TpmChallengeKeyImpl::OnSignChallengeDone,
                                 weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeyImpl::OnSignChallengeDone(
    const TpmChallengeKeyResult& sign_challenge_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!register_key_ || !sign_challenge_result.IsSuccess()) {
    std::move(callback_).Run(sign_challenge_result);
    return;
  }

  tpm_challenge_key_subtle_->StartRegisterKeyStep(
      base::BindOnce(&TpmChallengeKeyImpl::OnRegisterKeyDone,
                     weak_factory_.GetWeakPtr(), sign_challenge_result));
}

void TpmChallengeKeyImpl::OnRegisterKeyDone(
    const TpmChallengeKeyResult& challenge_response,
    const TpmChallengeKeyResult& register_key_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If StartRegisterKeyStep failed, |register_key_result| contains an error
  // about it.
  if (!register_key_result.IsSuccess()) {
    std::move(callback_).Run(register_key_result);
    return;
  }

  // All steps succeeded, return the final result. The challenge response that
  // is expected from |BuildResponse| was received in |OnSignChallengeDone|, so
  // return it now.
  std::move(callback_).Run(challenge_response);
}

}  // namespace attestation
}  // namespace ash
