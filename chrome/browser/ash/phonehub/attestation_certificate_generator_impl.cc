// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation_certificate_generator_impl.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
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
  GenerateCertificate();
}

AttestationCertificateGeneratorImpl::~AttestationCertificateGeneratorImpl() =
    default;

void AttestationCertificateGeneratorImpl::RetrieveCertificate(
    OnCertificateRetrievedCallback callback) {
  // TODO(b/278933392): Add a daily task to update certificate.
  // No certificates are cached or existing certificate was generated than 24
  // hours ago. Generating new ones.
  if (last_attestation_certificate_generated_time_.is_null() ||
      (last_attestation_certificate_generated_time_ - base::Time::Now())
              .InHours() > 24) {
    callback_ = std::move(callback);
    GenerateCertificate();
    return;
  }
  std::move(callback).Run(attestation_certs_, is_valid_);
}

void AttestationCertificateGeneratorImpl::GenerateCertificate() {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);

  if (user == nullptr) {
    OnAttestationCertificateGenerated({}, false);
    return;
  }

  const device_sync::CryptAuthKey* user_key_pair = key_registry_->GetActiveKey(
      device_sync::CryptAuthKeyBundle::Name::kUserKeyPair);

  if (user_key_pair == nullptr) {
    OnAttestationCertificateGenerated({}, false);
    return;
  }

  soft_bind_attestation_flow_->GetCertificate(
      base::BindOnce(&AttestationCertificateGeneratorImpl::
                         OnAttestationCertificateGenerated,
                     weak_ptr_factory_.GetWeakPtr()),
      user ? user->GetAccountId() : EmptyAccountId(),
      user_key_pair->public_key());
}

void AttestationCertificateGeneratorImpl::OnAttestationCertificateGenerated(
    const std::vector<std::string>& attestation_certs,
    bool is_valid) {
  attestation_certs_ = attestation_certs;
  is_valid_ = is_valid;
  last_attestation_certificate_generated_time_ = base::Time::Now();
  if (!callback_.is_null()) {
    std::move(callback_).Run(attestation_certs_, is_valid_);
    callback_.Reset();
  }
}
}  // namespace ash::phonehub
