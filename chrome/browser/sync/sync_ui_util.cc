// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include <utility>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
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
#include "net/base/url_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace sync_ui_util {

namespace {

StatusLabels GetStatusForUnrecoverableError(bool is_user_signout_allowed) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  int status_label_string_id =
      is_user_signout_allowed
          ? IDS_SYNC_STATUS_UNRECOVERABLE_ERROR
          :
          // The message for managed accounts is the same as that on ChromeOS.
          IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT;
#else
  int status_label_string_id =
      IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT;
#endif

  return {SYNC_ERROR, status_label_string_id, IDS_SYNC_RELOGIN_BUTTON,
          REAUTHENTICATE};
}

// Depending on the authentication state, returns labels to be used to display
// information about the sync status.
StatusLabels GetStatusForAuthError(const GoogleServiceAuthError& auth_error) {
  switch (auth_error.state()) {
    case GoogleServiceAuthError::NONE:
      NOTREACHED();
      break;
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      return {SYNC_ERROR, IDS_SYNC_SERVICE_UNAVAILABLE,
              IDS_SETTINGS_EMPTY_STRING, NO_ACTION};
    case GoogleServiceAuthError::CONNECTION_FAILED:
      // Note that there is little the user can do if the server is not
      // reachable. Since attempting to re-connect is done automatically by
      // the Syncer, we do not show the (re)login link.
      return {SYNC_ERROR, IDS_SYNC_SERVER_IS_UNREACHABLE,
              IDS_SETTINGS_EMPTY_STRING, NO_ACTION};
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::SERVICE_ERROR:
    default:
      return {SYNC_ERROR, IDS_SYNC_RELOGIN_ERROR, IDS_SYNC_RELOGIN_BUTTON,
              REAUTHENTICATE};
  }

  NOTREACHED();
  return StatusLabels();
}

StatusLabels GetStatusLabelsImpl(const syncer::SyncService* service,
                                 bool is_user_signout_allowed,
                                 const GoogleServiceAuthError& auth_error) {
  DCHECK(service);

  if (!service->IsAuthenticatedAccountPrimary()) {
    return {PRE_SYNCED, IDS_SETTINGS_EMPTY_STRING, IDS_SETTINGS_EMPTY_STRING,
            NO_ACTION};
  }

  // If local Sync were enabled, then the SyncService shouldn't report having a
  // primary (or any) account.
  DCHECK(!service->IsLocalSyncEnabled());

  // First check if Chrome needs to be updated.
  if (service->RequiresClientUpgrade()) {
    return {SYNC_ERROR, IDS_SYNC_UPGRADE_CLIENT, IDS_SYNC_UPGRADE_CLIENT_BUTTON,
            UPGRADE_CLIENT};
  }

  // Then check for an unrecoverable error.
  if (service->HasUnrecoverableError()) {
    return GetStatusForUnrecoverableError(is_user_signout_allowed);
  }

  // Then check for an auth error.
  if (auth_error.state() != GoogleServiceAuthError::NONE) {
    return GetStatusForAuthError(auth_error);
  }

  // Check if Sync is disabled by policy.
  if (service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    // TODO(crbug.com/911153): Is SYNCED correct for this case?
    return {SYNCED, IDS_SIGNED_IN_WITH_SYNC_DISABLED_BY_POLICY,
            IDS_SETTINGS_EMPTY_STRING, NO_ACTION};
  }

  // Check to see if sync has been disabled via the dashboard and needs to be
  // set up once again.
  if (!service->GetUserSettings()->IsSyncRequested()) {
    return {SYNC_ERROR, IDS_SIGNED_IN_WITH_SYNC_STOPPED_VIA_DASHBOARD,
            IDS_SETTINGS_EMPTY_STRING, NO_ACTION};
  }

  if (service->GetUserSettings()->IsFirstSetupComplete()) {
    // Check for a passphrase error.
    if (service->GetUserSettings()
            ->IsPassphraseRequiredForPreferredDataTypes()) {
      // TODO(mastiz): This should return PASSWORDS_ONLY_SYNC_ERROR if only
      // passwords are encrypted as per IsEncryptEverythingEnabled().
      return {SYNC_ERROR, IDS_SYNC_STATUS_NEEDS_PASSWORD,
              IDS_SYNC_STATUS_NEEDS_PASSWORD_BUTTON, ENTER_PASSPHRASE};
    }

    if (service->IsSyncFeatureActive() &&
        service->GetUserSettings()
            ->IsTrustedVaultKeyRequiredForPreferredDataTypes()) {
      return {service->GetUserSettings()->IsEncryptEverythingEnabled()
                  ? SYNC_ERROR
                  : PASSWORDS_ONLY_SYNC_ERROR,
              IDS_SETTINGS_EMPTY_STRING, IDS_SYNC_STATUS_NEEDS_KEYS_BUTTON,
              RETRIEVE_TRUSTED_VAULT_KEYS};
    }

    // At this point, there is no Sync error.
    if (service->IsSyncFeatureActive()) {
      return {SYNCED,
              service->GetUserSettings()->IsSyncEverythingEnabled()
                  ? IDS_SYNC_ACCOUNT_SYNCING
                  : IDS_SYNC_ACCOUNT_SYNCING_CUSTOM_DATA_TYPES,
              IDS_SETTINGS_EMPTY_STRING, NO_ACTION};
    } else {
      // Sync is still initializing.
      return {SYNCED, IDS_SETTINGS_EMPTY_STRING, IDS_SETTINGS_EMPTY_STRING,
              NO_ACTION};
    }
  }

  // If first setup is in progress, show an "in progress" message.
  if (service->IsSetupInProgress()) {
    return {PRE_SYNCED, IDS_SYNC_SETUP_IN_PROGRESS, IDS_SETTINGS_EMPTY_STRING,
            NO_ACTION};
  }

  // At this point we've ruled out all other cases - all that's left is a
  // missing Sync confirmation.
  DCHECK(ShouldRequestSyncConfirmation(service));
  return {SYNC_ERROR, IDS_SYNC_SETTINGS_NOT_CONFIRMED,
          IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON,
          CONFIRM_SYNC_SETTINGS};
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
  // Allow the window to close itself.
  params.created_with_opener = true;
  Navigate(&params);
}

// Returns true if the user has consented to browser sync-the-feature or
// Chrome OS sync.
bool HasUserOptedInToSync(const syncer::SyncUserSettings* settings) {
  if (settings->IsFirstSetupComplete())
    return true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::features::IsSplitSettingsSyncEnabled() &&
      settings->IsOsSyncFeatureEnabled()) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
}

}  // namespace

StatusLabels GetStatusLabels(syncer::SyncService* sync_service,
                             signin::IdentityManager* identity_manager,
                             bool is_user_signout_allowed) {
  if (!sync_service) {
    // This can happen if Sync is disabled via the command line.
    return {PRE_SYNCED, IDS_SETTINGS_EMPTY_STRING, IDS_SETTINGS_EMPTY_STRING,
            NO_ACTION};
  }
  DCHECK(identity_manager);
  CoreAccountInfo account_info = sync_service->GetAuthenticatedAccountInfo();
  GoogleServiceAuthError auth_error =
      identity_manager->GetErrorStateOfRefreshTokenForAccount(
          account_info.account_id);
  return GetStatusLabelsImpl(sync_service, is_user_signout_allowed, auth_error);
}

StatusLabels GetStatusLabels(Profile* profile) {
  DCHECK(profile);
  return GetStatusLabels(ProfileSyncServiceFactory::GetForProfile(profile),
                         IdentityManagerFactory::GetForProfile(profile),
                         signin_util::IsUserSignoutAllowedForProfile(profile));
}

MessageType GetStatus(Profile* profile) {
  return GetStatusLabels(profile).message_type;
}

AvatarSyncErrorType GetAvatarSyncErrorType(Profile* profile) {
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
      return MANAGED_USER_UNRECOVERABLE_ERROR;
    }
    return UNRECOVERABLE_ERROR;
  }

  // Check for an auth error.
  CoreAccountInfo account_info = service->GetAuthenticatedAccountInfo();
  GoogleServiceAuthError auth_error =
      IdentityManagerFactory::GetForProfile(profile)
          ->GetErrorStateOfRefreshTokenForAccount(account_info.account_id);

  if (auth_error.state() != GoogleServiceAuthError::State::NONE) {
    return AUTH_ERROR;
  }

  // Check if the Chrome client needs to be updated.
  if (service->RequiresClientUpgrade()) {
    return UPGRADE_CLIENT_ERROR;
  }

  // Check for a sync passphrase error.
  if (ShouldShowPassphraseError(service)) {
    return PASSPHRASE_ERROR;
  }

  // Check for a sync confirmation error.
  if (ShouldRequestSyncConfirmation(service)) {
    return SETTINGS_UNCONFIRMED_ERROR;
  }

  // Check for sync encryption keys missing.
  if (ShouldShowSyncKeysMissingError(service)) {
    return service->GetUserSettings()->IsEncryptEverythingEnabled()
               ? TRUSTED_VAULT_KEY_MISSING_FOR_EVERYTHING_ERROR
               : TRUSTED_VAULT_KEY_MISSING_FOR_PASSWORDS_ERROR;
  }

  // Check for trusted vault recoverability state.
  if (ShouldShowTrustedVaultDegradedRecoverabilityError(service)) {
    return service->GetUserSettings()->IsEncryptEverythingEnabled()
               ? TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING_ERROR
               : TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS_ERROR;
  }

  // There is no error.
  return NO_SYNC_ERROR;
}

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
  const syncer::SyncUserSettings* settings = service->GetUserSettings();
  return HasUserOptedInToSync(settings) &&
         settings->IsPassphraseRequiredForPreferredDataTypes();
}

bool ShouldShowSyncKeysMissingError(const syncer::SyncService* service) {
  const syncer::SyncUserSettings* settings = service->GetUserSettings();
  return HasUserOptedInToSync(settings) &&
         settings->IsTrustedVaultKeyRequiredForPreferredDataTypes();
}

bool ShouldShowTrustedVaultDegradedRecoverabilityError(
    const syncer::SyncService* service) {
  const syncer::SyncUserSettings* settings = service->GetUserSettings();
  return HasUserOptedInToSync(settings) &&
         settings->IsTrustedVaultRecoverabilityDegraded();
}

void OpenTabForSyncKeyRetrieval(
    Browser* browser,
    syncer::KeyRetrievalTriggerForUMA key_retrieval_trigger) {
  RecordKeyRetrievalTrigger(key_retrieval_trigger);
  const GURL continue_url =
      GURL(UIThreadSearchTermsData().GoogleBaseURLValue());
  GURL retrieval_url = GaiaUrls::GetInstance()->signin_chrome_sync_keys_url();
  if (continue_url.is_valid()) {
    retrieval_url = net::AppendQueryParameter(retrieval_url, "continue",
                                              continue_url.spec());
  }
  OpenTabForSyncKeyRetrievalWithURL(browser, retrieval_url);
}

void OpenTabForSyncKeyRetrievalWithURLForTesting(Browser* browser,
                                                 const GURL& url) {
  OpenTabForSyncKeyRetrievalWithURL(browser, url);
}

}  // namespace sync_ui_util
