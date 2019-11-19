// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
#define CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_

#include "base/strings/string16.h"
#include "build/build_config.h"

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
  PRE_SYNCED,  // User has not set up sync.
  SYNCED,      // We are synced and authenticated to a gmail account.
  SYNC_ERROR,  // A sync error (such as invalid credentials) has occurred.
};

// The action associated with the sync status.
enum ActionType {
  NO_ACTION,              // No action to take.
  REAUTHENTICATE,         // User needs to reauthenticate.
  SIGNOUT_AND_SIGNIN,     // User needs to sign out and sign in.
  UPGRADE_CLIENT,         // User needs to upgrade the client.
  ENTER_PASSPHRASE,       // User needs to enter their passphrase.
  CONFIRM_SYNC_SETTINGS,  // User needs to confirm sync settings.
};

// Sync errors that should be exposed to the user through the avatar button.
enum AvatarSyncErrorType {
  NO_SYNC_ERROR,                     // No sync error.
  MANAGED_USER_UNRECOVERABLE_ERROR,  // Unrecoverable error for managed users.
  UNRECOVERABLE_ERROR,               // Unrecoverable error for regular users.
  AUTH_ERROR,                        // Authentication error.
  UPGRADE_CLIENT_ERROR,              // Out-of-date client error.
  PASSPHRASE_ERROR,                  // Sync passphrase error.
  TRUSTED_VAULT_KEY_MISSING_ERROR,   // Trusted vault keys missing.
  SETTINGS_UNCONFIRMED_ERROR,        // Sync settings dialog not confirmed yet.
};

// Returns the high-level sync status, and populates status and link label
// strings for the current sync status by querying |sync_service| and
// |identity_manager|. Any of |status_label|, |link_label|, and |action_type|
// may be null if the caller isn't interested in it.
MessageType GetStatusLabels(syncer::SyncService* sync_service,
                            signin::IdentityManager* identity_manager,
                            bool is_user_signout_allowed,
                            base::string16* status_label,
                            base::string16* link_label,
                            ActionType* action_type);

// Returns the high-level sync status, and populates status and link label
// strings for the current sync status by querying |profile|. This is a
// convenience version of GetStatusLabels that use the |sync_service| and
// |identity_manager| associated to |profile| via their respective factories.
// Any of |status_label|, |link_label|, and |action_type| may be null if the
// caller isn't interested in it.
MessageType GetStatusLabels(Profile* profile,
                            base::string16* status_label,
                            base::string16* link_label,
                            ActionType* action_type);

// Convenience version of GetStatusLabels for when you're not interested in the
// actual labels, only in the return value.
MessageType GetStatus(Profile* profile);

#if !defined(OS_CHROMEOS)
// Gets the error message and button label for the sync errors that should be
// exposed to the user through the titlebar avatar button.
AvatarSyncErrorType GetMessagesForAvatarSyncError(
    Profile* profile,
    int* content_string_id,
    int* button_string_id);
#endif

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

// Opens a tab to trigger a reauth to retrieve the trusted vault keys.
void OpenTabForSyncKeyRetrieval(Browser* browser);

// Testing-only variant of the above which allows the caller to specify the
// URL.
void OpenTabForSyncKeyRetrievalWithURLForTesting(Browser* browser,
                                                 const GURL& url);

}  // namespace sync_ui_util

#endif  // CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
