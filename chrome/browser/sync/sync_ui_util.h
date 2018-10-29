// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
#define CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_

#include "base/strings/string16.h"
#include "build/build_config.h"

class Profile;
class SigninManagerBase;

namespace browser_sync {
class ProfileSyncService;
}  // namespace browser_sync

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

enum StatusLabelStyle {
  PLAIN_TEXT,  // Label will be plain-text only.
  WITH_HTML    // Label may contain an HTML-formatted link.
};

// Sync errors that should be exposed to the user through the avatar button.
enum AvatarSyncErrorType {
  NO_SYNC_ERROR,                     // No sync error.
  MANAGED_USER_UNRECOVERABLE_ERROR,  // Unrecoverable error for managed users.
  UNRECOVERABLE_ERROR,               // Unrecoverable error for regular users.
  SUPERVISED_USER_AUTH_ERROR,        // Auth token error for supervised users.
  AUTH_ERROR,                        // Authentication error.
  UPGRADE_CLIENT_ERROR,              // Out-of-date client error.
  PASSPHRASE_ERROR,                  // Sync passphrase error.
  SETTINGS_UNCONFIRMED_ERROR,        // Sync settings dialog not confirmed yet.
};

// TODO(akalin): audit the use of ProfileSyncService* service below,
// and use const ProfileSyncService& service where possible.

// Create status and link labels for the current status labels and link text
// by querying |service|.
MessageType GetStatusLabels(Profile* profile,
                            const browser_sync::ProfileSyncService* service,
                            const SigninManagerBase& signin,
                            base::string16* status_label,
                            base::string16* link_label,
                            ActionType* action_type);

#if !defined(OS_CHROMEOS)
// Gets the error message and button label for the sync errors that should be
// exposed to the user through the titlebar avatar button.
AvatarSyncErrorType GetMessagesForAvatarSyncError(
    Profile* profile,
    const SigninManagerBase& signin,
    int* content_string_id,
    int* button_string_id);
#endif

MessageType GetStatus(Profile* profile,
                      const browser_sync::ProfileSyncService* service,
                      const SigninManagerBase& signin);

// Returns whether it makes sense to show a Sync passphrase error UI, i.e.
// whether a missing passphrase is preventing Sync from fully starting up.
bool ShouldShowPassphraseError(const browser_sync::ProfileSyncService* service);

}  // namespace sync_ui_util

#endif  // CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
