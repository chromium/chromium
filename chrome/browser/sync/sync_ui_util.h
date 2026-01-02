// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
#define CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_

#include <optional>

#include "build/build_config.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"

class Profile;

#if !BUILDFLAG(IS_ANDROID)
class Browser;
#endif

namespace syncer {
class SyncService;
}  // namespace syncer

// Utility functions to gather current sync status information from the sync
// service and constructs messages suitable for showing in UI.

enum class SyncStatusMessageType {
  // User has not set up sync.
  kPreSynced,
  // We are synced and authenticated to a gmail account.
  kSynced,
  // A sync error (such as invalid credentials) has occurred.
  kSyncError,
  // Same as kSyncError but affecting passwords only.
  kPasswordsOnlySyncError,
};

// The action associated with the sync status in settings.
enum class SyncStatusActionType {
  // No action to take.
  kNoAction,
  // User needs to reauthenticate.
  kReauthenticate,
  // User needs to upgrade the client.
  kUpgradeClient,
  // User needs to enter their passphrase.
  kEnterPassphrase,
  // User needs to go through key retrieval.
  kRetrieveTrustedVaultKeys,
  // User needs to confirm sync settings.
  kConfirmSyncSettings,
  // User needs to see the help article for bookmarks limit.
  kShowBookmarksLimitHelpArticle,
};

struct SyncStatusLabels {
  SyncStatusMessageType message_type = SyncStatusMessageType::kPreSynced;
  int status_label_string_id = 0;
  int button_string_id = 0;
  int secondary_button_string_id = 0;
  SyncStatusActionType action_type = SyncStatusActionType::kNoAction;
};

#if !BUILDFLAG(IS_ANDROID)
SyncStatusLabels GetSyncStatusLabelsForSettings(
    const syncer::SyncService* service);

// `error` must not be `kNone`.
// If `support_title_case` is true, the string may be capitalized depending on
// platform and language. If false, sentence casing is used.
int GetSyncErrorButtonStringId(syncer::SyncService::UserActionableError error,
                               bool support_title_case);

// `error` must not be `kNone`.
SyncStatusLabels GetAvatarSyncErrorLabelsForSettings(
    Profile* profile,
    syncer::SyncService::UserActionableError error);

// This returns the string to be shown both as the tooltip of the avatar button,
// and in the profile menu body (the menu opened by clicking the avatar button).
// `error` must not be `kNone`.
std::u16string GetAvatarSyncErrorDescription(
    syncer::SyncService::UserActionableError error,
    const std::string& user_email);
#endif

// Whether sync is currently blocked from starting because the sync
// confirmation dialog hasn't been shown. Note that once the dialog is
// showing (i.e. IsSetupInProgress() is true), this will return false.
bool ShouldRequestSyncConfirmation(const syncer::SyncService* service);

// Returns whether it makes sense to show a Sync passphrase error UI, i.e.
// whether a missing passphrase is preventing Sync from fully starting up.
bool ShouldShowSyncPassphraseError(const syncer::SyncService* service);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
// Shows the sync passphrase dialog and attempts decrypting the data using the
// provided passphrase.
void ShowSyncPassphraseDialogAndDecryptData(Browser& browser);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
// Opens a tab for the purpose of retrieving the trusted vault keys, which
// usually requires a reauth.
void OpenTabForSyncKeyRetrieval(
    Browser* browser,
    trusted_vault::TrustedVaultUserActionTriggerForUMA trigger);

// Opens a tab for the purpose of improving the recoverability of the trusted
// vault keys, which usually requires a reauth.
void OpenTabForSyncKeyRecoverabilityDegraded(
    Browser* browser,
    trusted_vault::TrustedVaultUserActionTriggerForUMA trigger);
#endif  // !BUILDFLAG(IS_ANDROID)

#endif  // CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
