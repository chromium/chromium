// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/ode/chrome_passkey_on_device_encryption_state_tracker.h"

#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "components/sync/service/sync_service.h"
#include "components/webauthn/core/browser/passkey_model.h"

namespace password_manager {

ChromePasskeyOnDeviceEncryptionStateTracker::
    ChromePasskeyOnDeviceEncryptionStateTracker(
        syncer::SyncService* sync_service,
        EnclaveManagerInterface* enclave_manager,
        webauthn::PasskeyModel* passkey_model)
    : PasskeyOnDeviceEncryptionStateTracker(sync_service, passkey_model) {
  if (enclave_manager) {
    enclave_manager_observation_.Observe(enclave_manager);
  }
  ComputeState();
}

ChromePasskeyOnDeviceEncryptionStateTracker::
    ~ChromePasskeyOnDeviceEncryptionStateTracker() = default;

void ChromePasskeyOnDeviceEncryptionStateTracker::OnStateUpdated() {
  ComputeState();
}

OnDeviceEncryptionState
ChromePasskeyOnDeviceEncryptionStateTracker::GetPlatformState() const {
  const EnclaveManagerInterface* enclave =
      enclave_manager_observation_.GetSource();
  if (!enclave || !enclave->IsLoaded()) {
    return OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable;
  }

  return enclave->IsReady() ? OnDeviceEncryptionState::kDeviceReady
                            : OnDeviceEncryptionState::kDeviceNotReady;
}

}  // namespace password_manager
