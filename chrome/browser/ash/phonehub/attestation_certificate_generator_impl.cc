// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation_certificate_generator_impl.h"
#include "chrome/browser/ash/attestation/soft_bind_attestation_flow.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry_impl.h"
#include "components/user_manager/user.h"

namespace ash::phonehub {

AttestationCertificateGeneratorImpl::AttestationCertificateGeneratorImpl(
    Profile* profile,
    std::unique_ptr<attestation::SoftBindAttestationFlow>
        soft_bind_attestation_flow)
    : soft_bind_attestation_flow_(std::move(soft_bind_attestation_flow)),
      profile_(profile) {
  auto key_registry = device_sync::CryptAuthKeyRegistryImpl::Factory::Create(
      profile->GetPrefs());
  key_registry_ = std::move(key_registry);
}

AttestationCertificateGeneratorImpl::~AttestationCertificateGeneratorImpl() =
    default;

void AttestationCertificateGeneratorImpl::GenerateCertificate(
    OnCertificateGeneratedCallback callback) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);

  if (user == nullptr) {
    std::move(callback).Run({}, false);
    return;
  }

  const device_sync::CryptAuthKey* user_key_pair = key_registry_->GetActiveKey(
      device_sync::CryptAuthKeyBundle::Name::kUserKeyPair);

  if (user_key_pair == nullptr) {
    std::move(callback).Run({}, false);
    return;
  }

  soft_bind_attestation_flow_->GetCertificate(
      std::move(callback), user ? user->GetAccountId() : EmptyAccountId(),
      user_key_pair->public_key());
}
}  // namespace ash::phonehub
