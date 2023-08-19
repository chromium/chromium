// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_CLEANUP_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_CLEANUP_MANAGER_H_

#include "base/scoped_observation.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"

namespace enterprise_connectors {

// This class is in charge of cleaning up attestation keys and certificates from
// the Device Trust Connector on unmanaged devices. When a user is removed, it
// will also try to remove the associated DTC keys if they exist.
class AshAttestationCleanupManager
    : public user_manager::UserManager::Observer {
 public:
  AshAttestationCleanupManager();
  AshAttestationCleanupManager(const AshAttestationCleanupManager&) = delete;

  ~AshAttestationCleanupManager() override;

  // user_manager::UserManager::Observer:
  void OnUserRemoved(const AccountId& account_id,
                     user_manager::UserRemovalReason reason) override;

 private:
  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_CLEANUP_MANAGER_H_
