// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/keystore_service_ash.h"

#include <utility>

#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/attestation/tpm_challenge_key.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace crosapi {

KeystoreServiceAsh::KeystoreServiceAsh(
    mojo::PendingReceiver<mojom::KeystoreService> receiver)
    : receiver_(this, std::move(receiver)) {}

KeystoreServiceAsh::~KeystoreServiceAsh() = default;

void KeystoreServiceAsh::ChallengeAttestationOnlyKeystore(
    const std::string& challenge,
    mojom::KeystoreType type,
    bool migrate,
    ChallengeAttestationOnlyKeystoreCallback callback) {
  if (!crosapi::mojom::IsKnownEnumValue(type)) {
    mojom::ChallengeAttestationOnlyKeystoreResultPtr result_ptr =
        mojom::ChallengeAttestationOnlyKeystoreResult::New();
    result_ptr->set_error_message("unsupported keystore type");
    std::move(callback).Run(std::move(result_ptr));
    return;
  }
  chromeos::attestation::AttestationKeyType key_type;
  switch (type) {
    case mojom::KeystoreType::kUser:
      key_type = chromeos::attestation::KEY_USER;
      break;
    case mojom::KeystoreType::kDevice:
      key_type = chromeos::attestation::KEY_DEVICE;
      break;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();

  std::unique_ptr<chromeos::attestation::TpmChallengeKey> challenge_key =
      chromeos::attestation::TpmChallengeKeyFactory::Create();
  chromeos::attestation::TpmChallengeKey* challenge_key_ptr =
      challenge_key.get();
  outstanding_challenges_.push_back(std::move(challenge_key));
  //  TODO(https://crbug.com/1127505): Plumb |migrate| param.
  challenge_key_ptr->BuildResponse(
      key_type, profile,
      base::BindOnce(&KeystoreServiceAsh::DidChallengeAttestationOnlyKeystore,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     challenge_key_ptr),
      challenge,
      /*register_key=*/false, /*key_name_for_spkac=*/"");
}

void KeystoreServiceAsh::DidChallengeAttestationOnlyKeystore(
    ChallengeAttestationOnlyKeystoreCallback callback,
    void* challenge_key_ptr,
    const chromeos::attestation::TpmChallengeKeyResult& result) {
  mojom::ChallengeAttestationOnlyKeystoreResultPtr result_ptr =
      mojom::ChallengeAttestationOnlyKeystoreResult::New();
  if (result.IsSuccess()) {
    result_ptr->set_challenge_response(result.challenge_response);
  } else {
    result_ptr->set_error_message(result.GetErrorMessage());
  }
  std::move(callback).Run(std::move(result_ptr));

  // Remove the outstanding challenge_key object.
  bool found = false;
  for (auto it = outstanding_challenges_.begin();
       it != outstanding_challenges_.end(); ++it) {
    if (it->get() == challenge_key_ptr) {
      outstanding_challenges_.erase(it);
      found = true;
      break;
    }
  }
  DCHECK(found);
}

}  // namespace crosapi
