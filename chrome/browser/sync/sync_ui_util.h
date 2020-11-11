// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
#define CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_

#include "build/build_config.h"
#include "components/sync/driver/sync_service_utils.h"

class Browser;
class GURL;
class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

// Utility functions to gather current sync status information from the sync
// service and constructs messages suitable for showing in UI.
namespace sync_ui_util {

enum MessageType {
  // User has not set up sync.
  PRE_SYNCED,
  // We are synced and authenticated to a gmail account.
  SYNCED,
  // A sync error (such as invalid credentials) has occurred.
  SYNC_ERROR,
  // Same as SYNC_ERROR but affecting passwords only.
  PASSWORDS_ONLY_SYNC_ERROR,
};

// The action associated with the sync status in settings.
enum ActionType {
  // No action to take.
  NO_ACTION,
  // User needs to reauthenticate.
  REAUTHENTICATE,
  // User needs to sign out and sign in.
  SIGNOUT_AND_SIGNIN,
  // User needs to upgrade the client.
  UPGRADE_CLIENT,
  // User needs to enter their passphrase.
  ENTER_PASSPHRASE,
  // User needs to go through key retrieval.
  RETRIEVE_TRUSTED_VAULT_KEYS,
  // User needs to confirm sync settings.
  CONFIRM_SYNC_SETTINGS,
};

// Sync errors that should be exposed to the user through the avatar button.
enum AvatarSyncErrorType {
  // No sync error.
  NO_SYNC_ERROR,
  // Unrecoverable error for managed users.
  MANAGED_USER_UNRECOVERABLE_ERROR,
  // Unrecoverable error for regular users.
  UNRECOVERABLE_ERROR,
  // Authentication error.
  AUTH_ERROR,
  // Out-of-date client error.
  UPGRADE_CLIENT_ERROR,
  // Sync passphrase error.
  PASSPHRASE_ERROR,
  // Trusted vault keys missing for all sync datatypes (encrypt everything is
  // enabled).
  TRUSTED_VAULT_KEY_MISSING_FOR_EVERYTHING_ERROR,
  // Trusted vault keys missing for always-encrypted datatypes (passwords).
  TRUSTED_VAULT_KEY_MISSING_FOR_PASSWORDS_ERROR,
  // User needs to improve recoverability of the trusted vault (encrypt
  // everything is enabled).
  TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING_ERROR,
  // User needs to improve recoverability of the trusted vault (passwords).
  TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS_ERROR,
  // Sync settings dialog not confirmed yet.
  SETTINGS_UNCONFIRMED_ERROR,
};

struct StatusLabels {
  MessageType message_type;
  int status_label_string_id;
  int button_string_id;
  ActionType action_type;
};

// Returns the high-level sync status by querying |sync_service| and
// |identity_manager|.
StatusLabels GetStatusLabels(syncer::SyncService* sync_service,
                             signin::IdentityManager* identity_manager,
                             bool is_user_signout_allowed);

// Returns the high-level sync status by querying |profile|. This is a
// convenience version of GetStatusLabels that use the |sync_service| and
// |identity_manager| associated to |profile| via their respective factories.
StatusLabels GetStatusLabels(Profile* profile);

// Convenience version of GetStatusLabels for when you're not interested in the
// actual labels, only in the return value.
MessageType GetStatus(Profile* profile);

// Gets the error type (if any) that should be exposed to the user through the
// titlebar avatar button.
AvatarSyncErrorType GetAvatarSyncErrorType(Profile* profile);

// Whether sync is currently blocked from starting because the sync
// confirmation dialog hasn't been shown. Note that once the dialog is
// showing (i.e. IsSetupInProgress() is true), this will return false.
bool ShouldRequestSyncConfirmation(const syncer::SyncService* service);

// Returns whether it makes sense to show a Sync passphrase error UI, i.e.
// whether a missing passphrase is preventing Sync from fully starting up.
bool ShouldShowPassphraseError(const syncer::SyncService* service);

// Returns whether missing trusted vault keys is preventing sync from starting
// up encrypted datatypes.
bool ShouldShowSyncKeysMissingError(const syncer::SyncService* service);

// Returns whether user action is required to improve the recoverability of the
// trusted vault.
bool ShouldShowTrustedVaultDegradedRecoverabilityError(
    const syncer::SyncService* service);

// Opens a tab to trigger a reauth to retrieve the trusted vault keys.
void OpenTabForSyncKeyRetrieval(
    Browser* browser,
    syncer::KeyRetrievalTriggerForUMA key_retrieval_trigger);

// Testing-only variant of the above which allows the caller to specify the
// URL.
void OpenTabForSyncKeyRetrievalWithURLForTesting(Browser* browser,
                                                 const GURL& url);

}  // namespace sync_ui_util

#endif  // CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
