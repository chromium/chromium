// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"

namespace sync_ui_util {

namespace {

void GetStatusForUnrecoverableError(bool is_user_signout_allowed,
                                    base::string16* status_label,
                                    base::string16* link_label,
                                    ActionType* action_type) {
  if (status_label) {
#if !defined(OS_CHROMEOS)
    if (is_user_signout_allowed) {
      *status_label =
          l10n_util::GetStringUTF16(IDS_SYNC_STATUS_UNRECOVERABLE_ERROR);
    } else {
      // The message for managed accounts is the same as that on ChromeOS.
      *status_label = l10n_util::GetStringUTF16(
          IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT);
    }
#else
    *status_label = l10n_util::GetStringUTF16(
        IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT);
#endif
  }
  if (link_label) {
    *link_label = l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL);
  }
  if (action_type) {
    *action_type = REAUTHENTICATE;
  }
}

// Depending on the authentication state, returns labels to be used to display
// information about the sync status.
void GetStatusForAuthError(const GoogleServiceAuthError& auth_error,
                           base::string16* status_label,
                           base::string16* link_label,
                           ActionType* action_type) {
  switch (auth_error.state()) {
    case GoogleServiceAuthError::NONE:
      NOTREACHED();
      break;
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      if (status_label) {
        *status_label = l10n_util::GetStringUTF16(IDS_SYNC_SERVICE_UNAVAILABLE);
      }
      break;
    case GoogleServiceAuthError::CONNECTION_FAILED:
      if (status_label) {
        *status_label =
            l10n_util::GetStringUTF16(IDS_SYNC_SERVER_IS_UNREACHABLE);
      }
      // Note that there is little the user can do if the server is not
      // reachable. Since attempting to re-connect is done automatically by
      // the Syncer, we do not show the (re)login link.
      break;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::SERVICE_ERROR:
    default:
      if (status_label) {
        *status_label = l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_ERROR);
      }
      if (link_label) {
        *link_label = l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL);
      }
      if (action_type) {
        *action_type = REAUTHENTICATE;
      }
      break;
  }
}

MessageType GetStatusLabelsImpl(
    const syncer::SyncService* service,
    bool is_user_signout_allowed,
    const GoogleServiceAuthError& auth_error,
    base::string16* status_label,
    base::string16* link_label,
    ActionType* action_type) {
  DCHECK(service);

  if (!service->IsAuthenticatedAccountPrimary()) {
    return PRE_SYNCED;
  }

  // If local Sync were enabled, then the SyncService shouldn't report having a
  // primary (or any) account.
  DCHECK(!service->IsLocalSyncEnabled());

  // First check if Chrome needs to be updated.
  if (service->RequiresClientUpgrade()) {
    if (status_label) {
      *status_label = l10n_util::GetStringUTF16(IDS_SYNC_UPGRADE_CLIENT);
    }
    if (link_label) {
      *link_label =
          l10n_util::GetStringUTF16(IDS_SYNC_UPGRADE_CLIENT_LINK_LABEL);
    }
    if (action_type) {
      *action_type = UPGRADE_CLIENT;
    }
    return SYNC_ERROR;
  }

  // Then check for an unrecoverable error.
  if (service->HasUnrecoverableError()) {
    GetStatusForUnrecoverableError(is_user_signout_allowed, status_label,
                                   link_label, action_type);
    return SYNC_ERROR;
  }

  // Then check for an auth error.
  if (auth_error.state() != GoogleServiceAuthError::NONE) {
    GetStatusForAuthError(auth_error, status_label, link_label, action_type);
    return SYNC_ERROR;
  }

  // Check if Sync is disabled by policy.
  if (service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    if (status_label) {
      *status_label =
          l10n_util::GetStringUTF16(IDS_SIGNED_IN_WITH_SYNC_DISABLED_BY_POLICY);
    }
    // TODO(crbug.com/911153): Is SYNCED correct for this case?
    return SYNCED;
  }

  // Check to see if sync has been disabled via the dashboard and needs to be
  // set up once again.
  if (!service->GetUserSettings()->IsSyncRequested()) {
    if (status_label) {
      *status_label = l10n_util::GetStringUTF16(
          IDS_SIGNED_IN_WITH_SYNC_STOPPED_VIA_DASHBOARD);
    }

    return SYNC_ERROR;
  }

  if (service->GetUserSettings()->IsFirstSetupComplete()) {
    // Check for a passphrase error.
    if (service->GetUserSettings()
            ->IsPassphraseRequiredForPreferredDataTypes()) {
      if (status_label) {
        *status_label =
            l10n_util::GetStringUTF16(IDS_SYNC_STATUS_NEEDS_PASSWORD);
      }
      if (link_label) {
        *link_label = l10n_util::GetStringUTF16(
            IDS_SYNC_STATUS_NEEDS_PASSWORD_LINK_LABEL);
      }
      if (action_type) {
        *action_type = ENTER_PASSPHRASE;
      }
      return SYNC_ERROR;
    }

    // At this point, there is no Sync error.
    if (status_label) {
      if (service->IsSyncFeatureActive()) {
        *status_label = l10n_util::GetStringUTF16(
            service->GetUserSettings()->IsSyncEverythingEnabled()
                ? IDS_SYNC_ACCOUNT_SYNCING
                : IDS_SYNC_ACCOUNT_SYNCING_CUSTOM_DATA_TYPES);
      } else {
        // Sync is still initializing.
        *status_label = base::string16();
      }
    }
    return SYNCED;
  }

  // If first setup is in progress, show an "in progress" message.
  if (service->IsSetupInProgress()) {
    if (status_label) {
      *status_label = l10n_util::GetStringUTF16(IDS_SYNC_SETUP_IN_PROGRESS);
    }
    return PRE_SYNCED;
  }

  // At this point we've ruled out all other cases - all that's left is a
  // missing Sync confirmation.
  DCHECK(ShouldRequestSyncConfirmation(service));
  if (status_label) {
    *status_label = l10n_util::GetStringUTF16(IDS_SYNC_SETTINGS_NOT_CONFIRMED);
  }
  if (link_label) {
    *link_label = l10n_util::GetStringUTF16(
        IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON);
  }
  if (action_type) {
    *action_type = CONFIRM_SYNC_SETTINGS;
  }
  return SYNC_ERROR;
}

void FocusWebContents(Browser* browser) {
  auto* const contents = browser->tab_strip_model()->GetActiveWebContents();
  if (contents)
    contents->Focus();
}

void OpenTabForSyncKeyRetrievalWithURL(Browser* browser, const GURL& url) {
  DCHECK(browser);
  FocusWebContents(browser);

  NavigateParams params(GetSingletonTabNavigateParams(browser, url));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, std::move(params));
}

}  // namespace

MessageType GetStatusLabels(syncer::SyncService* sync_service,
                            signin::IdentityManager* identity_manager,
                            bool is_user_signout_allowed,
                            base::string16* status_label,
                            base::string16* link_label,
                            ActionType* action_type) {
  if (!sync_service) {
    // This can happen if Sync is disabled via the command line.
    return PRE_SYNCED;
  }
  DCHECK(identity_manager);
  CoreAccountInfo account_info = sync_service->GetAuthenticatedAccountInfo();
  GoogleServiceAuthError auth_error =
      identity_manager->GetErrorStateOfRefreshTokenForAccount(
          account_info.account_id);
  return GetStatusLabelsImpl(sync_service, is_user_signout_allowed, auth_error,
                             status_label, link_label, action_type);
}

MessageType GetStatusLabels(Profile* profile,
                            base::string16* status_label,
                            base::string16* link_label,
                            ActionType* action_type) {
  DCHECK(profile);
  return GetStatusLabels(ProfileSyncServiceFactory::GetForProfile(profile),
                         IdentityManagerFactory::GetForProfile(profile),
                         signin_util::IsUserSignoutAllowedForProfile(profile),
                         status_label, link_label, action_type);
}

MessageType GetStatus(Profile* profile) {
  return GetStatusLabels(profile, /*status_label=*/nullptr,
                         /*link_label=*/nullptr, /*action_type=*/nullptr);
}

#if !defined(OS_CHROMEOS)
AvatarSyncErrorType GetMessagesForAvatarSyncError(
    Profile* profile,
    int* content_string_id,
    int* button_string_id) {
  const syncer::SyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  // If there is no SyncService (probably because sync is disabled from the
  // command line), then there's no error to show.
  if (!service)
    return NO_SYNC_ERROR;

  // The order or priority is going to be: 1. Unrecoverable errors.
  // 2. Auth errors. 3. Outdated client errors. 4. Passphrase errors.
  // Note that an unrecoverable error is sometimes caused by the Chrome client
  // being outdated; that case is handled separately below.
  if (service->HasUnrecoverableError() && !service->RequiresClientUpgrade()) {
    // Display different messages and buttons for managed accounts.
    if (!signin_util::IsUserSignoutAllowedForProfile(profile)) {
      // For a managed user, the user is directed to the signout
      // confirmation dialogue in the settings page.
      *content_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNOUT_MESSAGE;
      *button_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNOUT_BUTTON;
      return MANAGED_USER_UNRECOVERABLE_ERROR;
    }
    // For a non-managed user, we sign out on the user's behalf and prompt
    // the user to sign in again.
    *content_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_AGAIN_MESSAGE;
    *button_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_AGAIN_BUTTON;
    return UNRECOVERABLE_ERROR;
  }

  // Check for an auth error.
  CoreAccountInfo account_info = service->GetAuthenticatedAccountInfo();
  GoogleServiceAuthError auth_error =
      IdentityManagerFactory::GetForProfile(profile)
          ->GetErrorStateOfRefreshTokenForAccount(account_info.account_id);

  if (auth_error.state() != GoogleServiceAuthError::State::NONE) {
    // The user can reauth to resolve the signin error.
    *content_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_MESSAGE;
    *button_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_BUTTON;
    return AUTH_ERROR;
  }

  // Check if the Chrome client needs to be updated.
  if (service->RequiresClientUpgrade()) {
    *content_string_id = IDS_SYNC_ERROR_USER_MENU_UPGRADE_MESSAGE;
    *button_string_id = IDS_SYNC_ERROR_USER_MENU_UPGRADE_BUTTON;
    return UPGRADE_CLIENT_ERROR;
  }

  // Check for a sync passphrase error.
  if (ShouldShowPassphraseError(service)) {
    *content_string_id = IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_MESSAGE;
    *button_string_id = IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_BUTTON;
    return PASSPHRASE_ERROR;
  }

  // Check for a sync confirmation error.
  if (ShouldRequestSyncConfirmation(service)) {
    *content_string_id = IDS_SYNC_SETTINGS_NOT_CONFIRMED;
    *button_string_id = IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON;
    return SETTINGS_UNCONFIRMED_ERROR;
  }

  // Check for sync encryption keys missing.
  if (ShouldShowSyncKeysMissingError(service)) {
    *content_string_id = IDS_SYNC_ERROR_USER_MENU_RETRIEVE_KEYS_MESSAGE;
    *button_string_id = IDS_SYNC_ERROR_USER_MENU_RETRIEVE_KEYS_BUTTON;
    return TRUSTED_VAULT_KEY_MISSING_ERROR;
  }

  // There is no error.
  return NO_SYNC_ERROR;
}
#endif  // !defined(OS_CHROMEOS)

bool ShouldRequestSyncConfirmation(const syncer::SyncService* service) {
  // This method mostly handles two situations:
  // 1. The initial Sync setup was aborted without actually disabling Sync
  //    again. That generally shouldn't happen, but it might if Chrome crashed
  //    while the setup was ongoing, or due to past bugs in the setup flow.
  // 2. Sync was reset from the dashboard. That usually signs out the user too,
  //    but it doesn't on ChromeOS, or for managed (enterprise) accounts where
  //    sign-out is prohibited.
  // Note that we do not check IsSyncRequested() here: In situation 1 it'd
  // usually be true, but in situation 2 it's false. Note that while there is a
  // primary account, IsSyncRequested() can only be false if Sync was reset from
  // the dashboard.
  return !service->IsLocalSyncEnabled() &&
         service->IsAuthenticatedAccountPrimary() &&
         !service->IsSetupInProgress() &&
         !service->GetUserSettings()->IsFirstSetupComplete();
}

bool ShouldShowPassphraseError(const syncer::SyncService* service) {
  return service->GetUserSettings()->IsFirstSetupComplete() &&
         service->GetUserSettings()
             ->IsPassphraseRequiredForPreferredDataTypes();
}

bool ShouldShowSyncKeysMissingError(const syncer::SyncService* service) {
  return service->GetUserSettings()->IsFirstSetupComplete() &&
         service->GetUserSettings()
             ->IsTrustedVaultKeyRequiredForPreferredDataTypes();
}

void OpenTabForSyncKeyRetrieval(Browser* browser) {
  OpenTabForSyncKeyRetrievalWithURL(
      browser, GaiaUrls::GetInstance()->signin_chrome_sync_keys_url());
}

void OpenTabForSyncKeyRetrievalWithURLForTesting(Browser* browser,
                                                 const GURL& url) {
  OpenTabForSyncKeyRetrievalWithURL(browser, url);
}

}  // namespace sync_ui_util
