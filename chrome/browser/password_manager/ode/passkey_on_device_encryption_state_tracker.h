// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ODE_PASSKEY_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ODE_PASSKEY_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_

#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"

namespace password_manager {

// Monitors the on-device encryption state for passkeys.
// TODO(crbug.com/540854648): This class observes `SyncService` and
// `PasswordTrustedVaultOnDeviceEncryptionStateTracker` also observes
// `SyncService`. We need to consider whether it is worth moving the observation
// of the common dependencies to a common super class.
class PasskeyOnDeviceEncryptionStateTracker
    : public OnDeviceEncryptionStateTracker,
      public syncer::SyncServiceObserver,
      public EnclaveManagerInterface::Observer,
      public webauthn::PasskeyModel::Observer {
 public:
  PasskeyOnDeviceEncryptionStateTracker(
      syncer::SyncService* sync_service,
      EnclaveManagerInterface* enclave_manager,
      webauthn::PasskeyModel* passkey_model);

  PasskeyOnDeviceEncryptionStateTracker(
      const PasskeyOnDeviceEncryptionStateTracker&) = delete;
  PasskeyOnDeviceEncryptionStateTracker& operator=(
      const PasskeyOnDeviceEncryptionStateTracker&) = delete;

  ~PasskeyOnDeviceEncryptionStateTracker() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // EnclaveManagerInterface::Observer:
  void OnStateUpdated() override;

  // webauthn::PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;
  void OnPasskeyModelIsReady(bool is_ready) override;

 private:
  // Computes the on-device encryption state at the current point in time, and
  // passes it to `OnDeviceEncryptionStateTracker::SetState` (which will cache
  // the current state and will notify observers if the state has changed).
  void ComputeState();

  syncer::SyncService* sync_service();
  EnclaveManagerInterface* enclave_manager();
  webauthn::PasskeyModel* passkey_model();

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};

  base::ScopedObservation<EnclaveManagerInterface,
                          EnclaveManagerInterface::Observer>
      enclave_manager_observation_{this};

  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      passkey_model_observation_{this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ODE_PASSKEY_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_
