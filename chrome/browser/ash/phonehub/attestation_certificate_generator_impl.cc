// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation_certificate_generator_impl.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/soft_bind_attestation_flow.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry_impl.h"
#include "components/user_manager/user.h"

namespace ash::phonehub {

namespace {
constexpr base::TimeDelta kOfflineRetryTimeout = base::Minutes(1);
}

AttestationCertificateGeneratorImpl::AttestationCertificateGeneratorImpl(
    Profile* profile,
    std::unique_ptr<attestation::SoftBindAttestationFlow>
        soft_bind_attestation_flow)
    : soft_bind_attestation_flow_(std::move(soft_bind_attestation_flow)),
      profile_(profile) {
  auto key_registry = device_sync::CryptAuthKeyRegistryImpl::Factory::Create(
      profile->GetPrefs());
  key_registry_ = std::move(key_registry);
  if (features::IsPhoneHubAttestationRetriesEnabled() &&
      NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
  }
  GenerateCertificate();
}

AttestationCertificateGeneratorImpl::~AttestationCertificateGeneratorImpl() {
  if (features::IsPhoneHubAttestationRetriesEnabled() &&
      NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
}

bool AttestationCertificateGeneratorImpl::
    ShouldRegenerateAttestationCertificate() {
  if (features::IsPhoneHubAttestationRetriesEnabled() && !is_valid_) {
    return true;
  }

  if (last_attestation_completed_time_.is_null()) {
    return true;
  }

  if ((base::Time::NowFromSystemTime() - last_attestation_completed_time_) >
      base::Hours(24)) {
    return true;
  }
  return false;
}

void AttestationCertificateGeneratorImpl::RetrieveCertificate() {
  // TODO(b/278933392): Add a daily task to update certificate.
  if (ShouldRegenerateAttestationCertificate()) {
    GenerateCertificate();
    return;
  }
  NotifyCertificateGenerated(attestation_certs_, is_valid_);
}

void AttestationCertificateGeneratorImpl::GenerateCertificate() {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);

  if (user == nullptr) {
    PA_LOG(WARNING) << __func__ << ": User unavailable for current profile.";
    OnAttestationCertificateGenerated({}, false);
    return;
  }

  const device_sync::CryptAuthKey* user_key_pair = key_registry_->GetActiveKey(
      device_sync::CryptAuthKeyBundle::Name::kUserKeyPair);

  if (user_key_pair == nullptr) {
    PA_LOG(WARNING) << __func__ << ": User missing key pair.";
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
  last_attestation_completed_time_ = base::Time::NowFromSystemTime();
  NotifyCertificateGenerated(attestation_certs_, is_valid_);
}

void AttestationCertificateGeneratorImpl::DefaultNetworkChanged(
    const NetworkState* network) {
  // Only retry when we have an active connected network.
  if (!network || !network->IsConnectedState()) {
    return;
  }

  // Throttle attempts to prevent too many requests.
  if ((base::Time::NowFromSystemTime() -
       last_attestation_attempt_from_network_change_time_) <
      kOfflineRetryTimeout) {
    return;
  }

  if (ShouldRegenerateAttestationCertificate()) {
    last_attestation_attempt_from_network_change_time_ =
        base::Time::NowFromSystemTime();
    GenerateCertificate();
  }
}

}  // namespace ash::phonehub
