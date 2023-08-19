// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_cleanup_manager.h"

#include "base/check_is_test.h"
#include "base/logging.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_subtle.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_service_impl.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/attestation/keystore.pb.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/user_manager/user_manager.h"

namespace enterprise_connectors {

AshAttestationCleanupManager::AshAttestationCleanupManager() {
  // Certificates need only to be cleaned up on unmanaged devices due to
  // stricter privacy constraints for user account removals. For managed
  // devices, certificates can still be cleaned up through a powerwash.
  if (ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    return;
  }

  if (!user_manager::UserManager::IsInitialized()) {
    CHECK_IS_TEST();
    return;
  }

  user_manager_observation_.Observe(user_manager::UserManager::Get());
}

AshAttestationCleanupManager::~AshAttestationCleanupManager() = default;

void AshAttestationCleanupManager::OnUserRemoved(
    const AccountId& account_id,
    user_manager::UserRemovalReason reason) {
  AshAttestationServiceImpl::KeyName key_name =
      AshAttestationServiceImpl::GetDeviceTrustConnectorUserKeyName(
          AshAttestationServiceImpl::Username(account_id.GetUserEmail()));

  ::attestation::DeleteKeysRequest request;
  request.set_username(std::string());
  request.set_key_label_match(key_name.value());
  request.set_match_behavior(
      ::attestation::DeleteKeysRequest::MATCH_BEHAVIOR_EXACT);

  auto callback = [](const AshAttestationServiceImpl::KeyName& key_name,
                     const ::attestation::DeleteKeysReply& reply) {
    if (reply.status() == ::attestation::STATUS_SUCCESS) {
      VLOG(1) << "Deleted attestation key " << key_name.value();
    }
  };
  ash::AttestationClient::Get()->DeleteKeys(request,
                                            base::BindOnce(callback, key_name));
}

}  // namespace enterprise_connectors
