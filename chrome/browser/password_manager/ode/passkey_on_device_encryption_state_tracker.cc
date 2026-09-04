// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/ode/passkey_on_device_encryption_state_tracker.h"

#include <vector>

#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"

namespace password_manager {

PasskeyOnDeviceEncryptionStateTracker::PasskeyOnDeviceEncryptionStateTracker(
    syncer::SyncService* sync_service,
    EnclaveManagerInterface* enclave_manager,
    webauthn::PasskeyModel* passkey_model) {
  if (sync_service) {
    sync_service_observation_.Observe(sync_service);
  }
  if (enclave_manager) {
    enclave_manager_observation_.Observe(enclave_manager);
  }
  if (passkey_model) {
    passkey_model_observation_.Observe(passkey_model);
  }
  ComputeState();
}

PasskeyOnDeviceEncryptionStateTracker::
    ~PasskeyOnDeviceEncryptionStateTracker() = default;

void PasskeyOnDeviceEncryptionStateTracker::OnStateChanged(
    syncer::SyncService* sync) {
  ComputeState();
}

void PasskeyOnDeviceEncryptionStateTracker::OnSyncShutdown(
    syncer::SyncService* sync) {
  sync_service_observation_.Reset();
  ComputeState();
}

void PasskeyOnDeviceEncryptionStateTracker::OnStateUpdated() {
  ComputeState();
}

void PasskeyOnDeviceEncryptionStateTracker::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  ComputeState();
}

void PasskeyOnDeviceEncryptionStateTracker::OnPasskeyModelShuttingDown() {
  passkey_model_observation_.Reset();
  ComputeState();
}

void PasskeyOnDeviceEncryptionStateTracker::OnPasskeyModelIsReady(
    bool is_ready) {
  ComputeState();
}

void PasskeyOnDeviceEncryptionStateTracker::ComputeState() {
  // The logic of deriving the state based on sync_service() is similar for
  // passkey state tracker and for password state tracker.
  // TODO(crbug.com/540854648): Consider moving the common logic to a base
  // class.
  if (!sync_service()) {
    SetState(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
    return;
  }
  if (sync_service()->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN)) {
    SetState(OnDeviceEncryptionState::kProfileNotSignedIn);
    return;
  }
  if (sync_service()->GetTransportState() ==
      syncer::SyncService::TransportState::PAUSED) {
    SetState(OnDeviceEncryptionState::kProfileSignInPending);
    return;
  }
  if (!sync_service()->IsEngineInitialized()) {
    SetState(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
    return;
  }

  // Verify whether the user disabled syncing of passwords and passkeys.
  syncer::SyncUserSettings* user_settings = sync_service()->GetUserSettings();
  if (!user_settings || !user_settings->GetSelectedTypes().Has(
                            syncer::UserSelectableType::kPasswords)) {
    // TODO(crbug.com/540854648): Consider introducing separate states for
    // cases when sync is disabled by a user or by an enterprise policy.
    SetState(OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled);
    return;
  }

  if (!passkey_model() || !passkey_model()->IsReady() || !enclave_manager() ||
      !enclave_manager()->IsLoaded()) {
    SetState(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
    return;
  }

  // If there are no passkeys, on-device encryption is treated as not enabled.
  // Note that this is an approximation because there might be users who have
  // joined the passkey security domain but currently have 0 passkeys in their
  // account; however, we assume the number of such users is low.
  // TODO(crbug.com/540854648): Consider using a more accurate approach for
  // checking whether the user joined the passkey security domain.
  if (passkey_model()->IsEmpty()) {
    SetState(OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);
    return;
  }

  if (enclave_manager()->IsReady()) {
    SetState(OnDeviceEncryptionState::kDeviceReady);
  } else {
    SetState(OnDeviceEncryptionState::kDeviceNotReady);
  }
}

syncer::SyncService* PasskeyOnDeviceEncryptionStateTracker::sync_service() {
  return sync_service_observation_.GetSource();
}

EnclaveManagerInterface*
PasskeyOnDeviceEncryptionStateTracker::enclave_manager() {
  return enclave_manager_observation_.GetSource();
}

webauthn::PasskeyModel* PasskeyOnDeviceEncryptionStateTracker::passkey_model() {
  return passkey_model_observation_.GetSource();
}

}  // namespace password_manager
