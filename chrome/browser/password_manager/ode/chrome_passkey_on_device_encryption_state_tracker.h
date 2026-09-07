// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ODE_CHROME_PASSKEY_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ODE_CHROME_PASSKEY_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "components/password_manager/core/browser/ode/passkey_on_device_encryption_state_tracker.h"

namespace syncer {
class SyncService;
}

namespace webauthn {
class PasskeyModel;
}

namespace password_manager {

// Monitors the on-device encryption state for passkeys on desktop.
class ChromePasskeyOnDeviceEncryptionStateTracker
    : public PasskeyOnDeviceEncryptionStateTracker,
      public EnclaveManagerInterface::Observer {
 public:
  ChromePasskeyOnDeviceEncryptionStateTracker(
      syncer::SyncService* sync_service,
      EnclaveManagerInterface* enclave_manager,
      webauthn::PasskeyModel* passkey_model);

  ChromePasskeyOnDeviceEncryptionStateTracker(
      const ChromePasskeyOnDeviceEncryptionStateTracker&) = delete;
  ChromePasskeyOnDeviceEncryptionStateTracker& operator=(
      const ChromePasskeyOnDeviceEncryptionStateTracker&) = delete;

  ~ChromePasskeyOnDeviceEncryptionStateTracker() override;

  // EnclaveManagerInterface::Observer:
  void OnStateUpdated() override;

 private:
  // PasskeyOnDeviceEncryptionStateTracker:
  OnDeviceEncryptionState GetPlatformState() const override;

  base::ScopedObservation<EnclaveManagerInterface,
                          EnclaveManagerInterface::Observer>
      enclave_manager_observation_{this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ODE_CHROME_PASSKEY_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_
