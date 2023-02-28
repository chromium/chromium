// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include <utility>

#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace {

SyncStatusLabels GetStatusForUnrecoverableError(
    bool is_user_clear_primary_account_allowed) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  int status_label_string_id =
      is_user_clear_primary_account_allowed
          ? IDS_SYNC_STATUS_UNRECOVERABLE_ERROR
          :
          // The message for managed accounts is the same as that on ChromeOS.
          IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT;
#else
  int status_label_string_id =
      IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT;
#endif

  return {SyncStatusMessageType::kSyncError, status_label_string_id,
          IDS_SYNC_RELOGIN_BUTTON, SyncStatusActionType::kReauthenticate};
}

SyncStatusLabels GetSyncStatusLabelsImpl(
    const syncer::SyncService* service,
    bool is_user_clear_primary_account_allowed,
    const GoogleServiceAuthError& auth_error) {
  DCHECK(service);
  DCHECK(!auth_error.IsTransientError());

  if (!service->HasSyncConsent()) {
    return {SyncStatusMessageType::kPreSynced, IDS_SETTINGS_EMPTY_STRING,
            IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction};
  }

  // If local Sync were enabled, then the SyncService shouldn't report having a
  // primary (or any) account.
  DCHECK(!service->IsLocalSyncEnabled());

  // First check if Chrome needs to be updated.
  if (service->RequiresClientUpgrade()) {
    return {SyncStatusMessageType::kSyncError, IDS_SYNC_UPGRADE_CLIENT,
            IDS_SYNC_UPGRADE_CLIENT_BUTTON,
            SyncStatusActionType::kUpgradeClient};
  }

  // Then check for an unrecoverable error.
  if (service->HasUnrecoverableError()) {
    return GetStatusForUnrecoverableError(
        is_user_clear_primary_account_allowed);
  }

  // Then check for an auth error.
  if (auth_error.state() != GoogleServiceAuthError::NONE) {
    DCHECK(auth_error.IsPersistentError());
    return {SyncStatusMessageType::kSyncError, IDS_SYNC_RELOGIN_ERROR,
            IDS_SYNC_RELOGIN_BUTTON, SyncStatusActionType::kReauthenticate};
  }

  // Check if Sync is disabled by policy.
  if (service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    // TODO(crbug.com/911153): Is SyncStatusMessageType::kSynced correct for
    // this case?
    return {SyncStatusMessageType::kSynced,
            IDS_SIGNED_IN_WITH_SYNC_DISABLED_BY_POLICY,
            IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction};
  }

  // Check to see if sync has been disabled via the dashboard and needs to be
  // set up once again.
  if (!service->GetUserSettings()->IsSyncRequested()) {
    return {SyncStatusMessageType::kSyncError,
            IDS_SIGNED_IN_WITH_SYNC_STOPPED_VIA_DASHBOARD,
            IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction};
  }

  if (service->GetUserSettings()->IsFirstSetupComplete()) {
    // Check for a passphrase error.
    if (service->GetUserSettings()
            ->IsPassphraseRequiredForPreferredDataTypes()) {
      // TODO(mastiz): This should return
      // SyncStatusMessageType::kPasswordsOnlySyncError if only passwords are
      // encrypted as per IsEncryptEverythingEnabled().
      return {SyncStatusMessageType::kSyncError, IDS_SYNC_STATUS_NEEDS_PASSWORD,
              IDS_SYNC_STATUS_NEEDS_PASSWORD_BUTTON,
              SyncStatusActionType::kEnterPassphrase};
    }

    if (service->IsSyncFeatureActive() &&
        service->GetUserSettings()
            ->IsTrustedVaultKeyRequiredForPreferredDataTypes()) {
      return {service->GetUserSettings()->IsEncryptEverythingEnabled()
                  ? SyncStatusMessageType::kSyncError
                  : SyncStatusMessageType::kPasswordsOnlySyncError,
              IDS_SETTINGS_EMPTY_STRING, IDS_SYNC_STATUS_NEEDS_KEYS_BUTTON,
              SyncStatusActionType::kRetrieveTrustedVaultKeys};
    }

    // At this point, there is no Sync error.
    if (service->IsSyncFeatureActive()) {
      return {SyncStatusMessageType::kSynced,
              service->GetUserSettings()->IsSyncEverythingEnabled()
                  ? IDS_SYNC_ACCOUNT_SYNCING
                  : IDS_SYNC_ACCOUNT_SYNCING_CUSTOM_DATA_TYPES,
              IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction};
    } else {
      // Sync is still initializing.
      return {SyncStatusMessageType::kSynced, IDS_SETTINGS_EMPTY_STRING,
              IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction};
    }
  }

  // If first setup is in progress, show an "in progress" message.
  if (service->IsSetupInProgress()) {
    return {SyncStatusMessageType::kPreSynced, IDS_SYNC_SETUP_IN_PROGRESS,
            IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction};
  }

  // At this point we've ruled out all other cases - all that's left is a
  // missing Sync confirmation.
  DCHECK(ShouldRequestSyncConfirmation(service));
  return {SyncStatusMessageType::kSyncError, IDS_SYNC_SETTINGS_NOT_CONFIRMED,
          IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON,
          SyncStatusActionType::kConfirmSyncSettings};
}

void FocusWebContents(Browser* browser) {
  content::WebContents* const contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (contents) {
    contents->Focus();
  }
}

void OpenTabForSyncTrustedVaultUserAction(Browser* browser, const GURL& url) {
  DCHECK(browser);
  FocusWebContents(browser);

  NavigateParams params(GetSingletonTabNavigateParams(browser, url));
  // Allow the window to close itself.
  params.opened_by_another_window = true;
  Navigate(&params);
}

absl::optional<AvatarSyncErrorType> GetTrustedVaultError(
    const syncer::SyncService* sync_service,
    const PrefService* pref_service) {
  if (ShouldShowSyncKeysMissingError(sync_service, pref_service)) {
    return sync_service->GetUserSettings()->IsEncryptEverythingEnabled()
               ? AvatarSyncErrorType::kTrustedVaultKeyMissingForEverythingError
               : AvatarSyncErrorType::kTrustedVaultKeyMissingForPasswordsError;
  }

  if (ShouldShowTrustedVaultDegradedRecoverabilityError(sync_service,
                                                        pref_service)) {
    return sync_service->GetUserSettings()->IsEncryptEverythingEnabled()
               ? AvatarSyncErrorType::
                     kTrustedVaultRecoverabilityDegradedForEverythingError
               : AvatarSyncErrorType::
                     kTrustedVaultRecoverabilityDegradedForPasswordsError;
  }

  return absl::nullopt;
}

}  // namespace

SyncStatusLabels GetSyncStatusLabels(
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    bool is_user_clear_primary_account_allowed) {
  if (!sync_service) {
    // This can happen if Sync is disabled via the command line.
    return {SyncStatusMessageType::kPreSynced, IDS_SETTINGS_EMPTY_STRING,
            IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction};
  }
  DCHECK(identity_manager);
  CoreAccountInfo account_info = sync_service->GetAccountInfo();
  GoogleServiceAuthError auth_error =
      identity_manager->GetErrorStateOfRefreshTokenForAccount(
          account_info.account_id);
  return GetSyncStatusLabelsImpl(
      sync_service, is_user_clear_primary_account_allowed, auth_error);
}

SyncStatusLabels GetSyncStatusLabels(Profile* profile) {
  DCHECK(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return GetSyncStatusLabels(
      SyncServiceFactory::GetForProfile(profile), identity_manager,
      ChromeSigninClientFactory::GetForProfile(profile)
          ->IsClearPrimaryAccountAllowed(identity_manager->HasPrimaryAccount(
              signin::ConsentLevel::kSync)));
}

SyncStatusMessageType GetSyncStatusMessageType(Profile* profile) {
  return GetSyncStatusLabels(profile).message_type;
}

absl::optional<AvatarSyncErrorType> GetAvatarSyncErrorType(Profile* profile) {
  const syncer::SyncService* service =
      SyncServiceFactory::GetForProfile(profile);
  if (!service) {
    return absl::nullopt;
  }

  if (!service->HasSyncConsent()) {
    // Only trusted vault errors can be shown if the account isn't a consented
    // primary account.
    // Note the condition checked is not IsFirstSetupComplete(), because the
    // setup incomplete case is treated separately below. See the comment in
    // ShouldRequestSyncConfirmation() about dashboard resets.
    return GetTrustedVaultError(service, profile->GetPrefs());
  }

  // RequiresClientUpgrade() is unrecoverable, but is treated separately below.
  if (service->HasUnrecoverableError() && !service->RequiresClientUpgrade()) {
    // Display different messages and buttons for managed accounts.
    if (!ChromeSigninClientFactory::GetForProfile(profile)
             ->IsClearPrimaryAccountAllowed(
                 IdentityManagerFactory::GetForProfile(profile)
                     ->HasPrimaryAccount(signin::ConsentLevel::kSync))) {
      return AvatarSyncErrorType::kManagedUserUnrecoverableError;
    }
    return AvatarSyncErrorType::kUnrecoverableError;
  }

  if (service->GetTransportState() ==
      syncer::SyncService::TransportState::PAUSED) {
    return AvatarSyncErrorType::kSyncPaused;
  }

  if (service->RequiresClientUpgrade()) {
    return AvatarSyncErrorType::kUpgradeClientError;
  }

  if (ShouldShowSyncPassphraseError(service)) {
    return AvatarSyncErrorType::kPassphraseError;
  }

  const absl::optional<AvatarSyncErrorType> trusted_vault_error =
      GetTrustedVaultError(service, profile->GetPrefs());
  if (trusted_vault_error) {
    return trusted_vault_error;
  }

  if (ShouldRequestSyncConfirmation(service)) {
    return AvatarSyncErrorType::kSettingsUnconfirmedError;
  }

  return absl::nullopt;
}

std::u16string GetAvatarSyncErrorDescription(AvatarSyncErrorType error,
                                             bool is_sync_feature_enabled) {
  switch (error) {
    case AvatarSyncErrorType::kSyncPaused:
      return l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SYNC_PAUSED_TITLE);
    case AvatarSyncErrorType::kTrustedVaultKeyMissingForPasswordsError:
      return l10n_util::GetStringUTF16(
          is_sync_feature_enabled
              ? IDS_SYNC_ERROR_PASSWORDS_USER_MENU_TITLE
              : IDS_SYNC_ERROR_PASSWORDS_USER_MENU_TITLE_SIGNED_IN_ONLY);
    case AvatarSyncErrorType::
        kTrustedVaultRecoverabilityDegradedForPasswordsError:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_ERROR_RECOVERABILITY_DEGRADED_FOR_PASSWORDS_USER_MENU_TITLE);
    case AvatarSyncErrorType::
        kTrustedVaultRecoverabilityDegradedForEverythingError:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_ERROR_RECOVERABILITY_DEGRADED_FOR_EVERYTHING_USER_MENU_TITLE);
    case AvatarSyncErrorType::kSettingsUnconfirmedError:
    case AvatarSyncErrorType::kManagedUserUnrecoverableError:
    case AvatarSyncErrorType::kUnrecoverableError:
    case AvatarSyncErrorType::kUpgradeClientError:
    case AvatarSyncErrorType::kPassphraseError:
    case AvatarSyncErrorType::kTrustedVaultKeyMissingForEverythingError:
      return l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_TITLE);
  }
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
  return !service->IsLocalSyncEnabled() && service->HasSyncConsent() &&
         !service->IsSetupInProgress() &&
         !service->GetUserSettings()->IsFirstSetupComplete();
}

bool ShouldShowSyncPassphraseError(const syncer::SyncService* service) {
  const syncer::SyncUserSettings* settings = service->GetUserSettings();
  return settings->IsFirstSetupComplete() &&
         settings->IsPassphraseRequiredForPreferredDataTypes();
}

bool ShouldShowSyncKeysMissingError(const syncer::SyncService* sync_service,
                                    const PrefService* pref_service) {
  const syncer::SyncUserSettings* settings = sync_service->GetUserSettings();
  if (!settings->IsTrustedVaultKeyRequiredForPreferredDataTypes()) {
    return false;
  }

  if (settings->IsFirstSetupComplete()) {
    return true;
  }

  // If sync is running in transport-only mode, every type is "preferred", so
  // IsTrustedVaultKeyRequiredForPreferredDataTypes() could return true even if
  // the user isn't trying to sync any of the encrypted types. The check below
  // tries to avoid showing an unexpected "You couldn't sync X" error in that
  // case. It works fine if IsEncryptEverythingEnabled() is false, since
  // PASSWORDS is the only one of AlwaysEncryptedUserTypes() currently
  // supporting transport mode. Otherwise, it should really be OR-ed with other
  // checks.
  // TODO(crbug.com/1134090): Fix the definition of preferred types for
  // transport mode so calling IsTrustedVaultKeyRequiredForPreferredDataTypes()
  // is enough.
  //
  // WARNING: Must match PasswordModelTypeController::GetPreconditionState().
  return password_manager::features_util::IsOptedInForAccountStorage(
      pref_service, sync_service);
}

bool ShouldShowTrustedVaultDegradedRecoverabilityError(
    const syncer::SyncService* sync_service,
    const PrefService* pref_service) {
  const syncer::SyncUserSettings* settings = sync_service->GetUserSettings();
  if (!settings->IsTrustedVaultRecoverabilityDegraded()) {
    return false;
  }

  if (settings->IsFirstSetupComplete()) {
    return true;
  }

  // In transport-only mode, IsTrustedVaultRecoverabilityDegraded() returns true
  // even if the user isn't trying to sync any of the encrypted types. The check
  // below tries to avoid unnecessarily showing the error in that case. It works
  // fine if IsEncryptEverythingEnabled() is false, since PASSWORDS is the only
  // one of AlwaysEncryptedUserTypes() currently supporting transport mode.
  // Otherwise, it should really be OR-ed with other checks.
  // TODO(crbug.com/1134090): Fix the definition of preferred types for
  // transport mode so calling IsTrustedVaultRecoverabilityDegraded() is enough
  // (SyncUserSettingsImpl::IsEncryptedDatatypeEnabled() relies on the preferred
  // types).
  //
  // WARNING: Must match PasswordModelTypeController::GetPreconditionState().
  return password_manager::features_util::IsOptedInForAccountStorage(
      pref_service, sync_service);
}

void OpenTabForSyncKeyRetrieval(
    Browser* browser,
    syncer::TrustedVaultUserActionTriggerForUMA trigger) {
  RecordKeyRetrievalTrigger(trigger);
  const GURL continue_url =
      GURL(UIThreadSearchTermsData().GoogleBaseURLValue());
  GURL retrieval_url =
      GaiaUrls::GetInstance()->signin_chrome_sync_keys_retrieval_url();
  if (continue_url.is_valid()) {
    retrieval_url = net::AppendQueryParameter(retrieval_url, "continue",
                                              continue_url.spec());
  }
  OpenTabForSyncTrustedVaultUserAction(browser, retrieval_url);
}

void OpenTabForSyncKeyRecoverabilityDegraded(
    Browser* browser,
    syncer::TrustedVaultUserActionTriggerForUMA trigger) {
  RecordRecoverabilityDegradedFixTrigger(trigger);
  const GURL continue_url =
      GURL(UIThreadSearchTermsData().GoogleBaseURLValue());
  GURL url = GaiaUrls::GetInstance()
                 ->signin_chrome_sync_keys_recoverability_degraded_url();
  if (continue_url.is_valid()) {
    url = net::AppendQueryParameter(url, "continue", continue_url.spec());
  }
  OpenTabForSyncTrustedVaultUserAction(browser, url);
}
