// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/ode/password_trusted_vault_on_device_encryption_state_tracker.h"

#include "components/sync/base/data_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace password_manager {

PasswordTrustedVaultOnDeviceEncryptionStateTracker::
    PasswordTrustedVaultOnDeviceEncryptionStateTracker(
        syncer::SyncService* sync_service) {
  if (sync_service) {
    sync_service_observation_.Observe(sync_service);
  }
  ComputeState();
}

PasswordTrustedVaultOnDeviceEncryptionStateTracker::
    ~PasswordTrustedVaultOnDeviceEncryptionStateTracker() = default;

void PasswordTrustedVaultOnDeviceEncryptionStateTracker::OnStateChanged(
    syncer::SyncService* sync) {
  ComputeState();
}

void PasswordTrustedVaultOnDeviceEncryptionStateTracker::OnSyncShutdown(
    syncer::SyncService* sync) {
  sync_service_observation_.Reset();
  ComputeState();
}

void PasswordTrustedVaultOnDeviceEncryptionStateTracker::ComputeState() {
  if (!sync_service() || !sync_service()->IsEngineInitialized()) {
    // While Sync engine is initializing the on-device encryption state is not
    // yet available.
    SetState(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
    return;
  }

  syncer::SyncUserSettings* user_settings = sync_service()->GetUserSettings();
  if (!user_settings || !user_settings->GetSelectedTypes().Has(
                            syncer::UserSelectableType::kPasswords)) {
    // TODO(crbug.com/540854648): Consider introducing separate states for
    // cases when sync is disabled by a user or by an enterprise policy.
    SetState(OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled);
    return;
  }

  if (user_settings->GetPassphraseType() !=
      syncer::PassphraseType::kTrustedVaultPassphrase) {
    SetState(OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);
    return;
  }

  if (!user_settings->IsTrustedVaultKeyRequiredForPreferredDataTypes() &&
      !sync_service()->GetActiveDataTypes().Has(syncer::PASSWORDS)) {
    // When Sync starts up, it asynchronously checks whether trusted vault keys
    // are available locally. While this check is in flight,
    // `IsTrustedVaultKeyRequiredForPreferredDataTypes()` temporarily returns
    // false and `syncer::PASSWORDS` is not active yet. This means that in this
    // case the on-device encryption state is not yet available.
    SetState(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
    return;
  }

  SetState(user_settings->IsTrustedVaultKeyRequiredForPreferredDataTypes()
               ? OnDeviceEncryptionState::kDeviceNotReady
               : OnDeviceEncryptionState::kDeviceReady);
}

syncer::SyncService*
PasswordTrustedVaultOnDeviceEncryptionStateTracker::sync_service() {
  return sync_service_observation_.GetSource();
}

}  // namespace password_manager
