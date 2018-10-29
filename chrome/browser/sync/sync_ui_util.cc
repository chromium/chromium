// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/protocol/sync_protocol_error.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/signin/signin_util.h"
#endif  // defined(OS_CHROMEOS)

using browser_sync::ProfileSyncService;

namespace sync_ui_util {

namespace {

// Returns the message that should be displayed when the user is authenticated
// and can connect to the sync server. If the user hasn't yet authenticated, an
// empty string is returned.
base::string16 GetSyncedStateStatusLabel(const ProfileSyncService* service,
                                         const SigninManagerBase& signin,
                                         StatusLabelStyle style,
                                         bool sync_everything) {
  if (!service || service->HasDisableReason(
                      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    // User is signed in, but sync is disabled.
    return l10n_util::GetStringUTF16(IDS_SIGNED_IN_WITH_SYNC_DISABLED);
  }
  if (service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_USER_CHOICE)) {
    // User is signed in, but sync has been stopped.
    return l10n_util::GetStringUTF16(IDS_SIGNED_IN_WITH_SYNC_SUPPRESSED);
  }
  if (!service->IsSyncFeatureActive()) {
    // User is not signed in, or sync is still initializing.
    return base::string16();
  }

  // Message may also carry additional advice with an HTML link, if acceptable.
  switch (style) {
    case PLAIN_TEXT:
      return l10n_util::GetStringUTF16(
          sync_everything ? IDS_SYNC_ACCOUNT_SYNCING
                          : IDS_SYNC_ACCOUNT_SYNCING_CUSTOM_DATA_TYPES);
    case WITH_HTML:
      return l10n_util::GetStringFUTF16(
          IDS_SYNC_ACCOUNT_SYNCING_WITH_MANAGE_LINK,
          base::ASCIIToUTF16(chrome::kSyncGoogleDashboardURL));
    default:
      NOTREACHED();
      return nullptr;
  }
}

void GetStatusForActionableError(const syncer::SyncProtocolError& error,
                                 base::string16* status_label,
                                 base::string16* link_label,
                                 ActionType* action_type) {
  DCHECK(status_label);
  DCHECK(link_label);
  switch (error.action) {
    case syncer::UPGRADE_CLIENT:
      status_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_UPGRADE_CLIENT));
      link_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_UPGRADE_CLIENT_LINK_LABEL));
      *action_type = UPGRADE_CLIENT;
      break;
    case syncer::ENABLE_SYNC_ON_ACCOUNT:
      status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_STATUS_ENABLE_SYNC_ON_ACCOUNT));
      break;
    default:
      status_label->clear();
      break;
  }
}

void GetStatusForUnrecoverableError(Profile* profile,
                                    const ProfileSyncService* service,
                                    base::string16* status_label,
                                    base::string16* link_label,
                                    ActionType* action_type) {
  // Unrecoverable error is sometimes accompanied by actionable error.
  // If status message is set display that message, otherwise show generic
  // unrecoverable error message.
  syncer::SyncStatus status;
  service->QueryDetailedSyncStatus(&status);
  GetStatusForActionableError(status.sync_protocol_error, status_label,
                              link_label, action_type);
  if (status_label->empty()) {
    *action_type = REAUTHENTICATE;
    link_label->assign(
        l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL));

#if !defined(OS_CHROMEOS)
    status_label->assign(l10n_util::GetStringUTF16(
        IDS_SYNC_STATUS_UNRECOVERABLE_ERROR));
    // The message for managed accounts is the same as that of the cros.
    if (!signin_util::IsUserSignoutAllowedForProfile(profile)) {
      status_label->assign(l10n_util::GetStringUTF16(
          IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT));
    }
#else
    status_label->assign(l10n_util::GetStringUTF16(
        IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT));
#endif
  }
}

// Depending on the authentication state, returns labels to be used to display
// information about the sync status.
void GetStatusForAuthError(Profile* profile,
                           const SigninManagerBase& signin_manager,
                           base::string16* status_label,
                           base::string16* link_label,
                           ActionType* action_type) {
  DCHECK(status_label);
  DCHECK(link_label);
  const GoogleServiceAuthError::State state =
      SigninErrorControllerFactory::GetForProfile(profile)->
          auth_error().state();
  switch (state) {
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_SERVICE_UNAVAILABLE));
      break;
    case GoogleServiceAuthError::CONNECTION_FAILED:
      status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_SERVER_IS_UNREACHABLE));
      // Note that there is little the user can do if the server is not
      // reachable. Since attempting to re-connect is done automatically by
      // the Syncer, we do not show the (re)login link.
      break;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
    default:
      status_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_ERROR));
      link_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL));
      *action_type = REAUTHENTICATE;
      break;
  }
}

// TODO(akalin): Write unit tests for these three functions below.

// status_label and link_label must either be both null or both non-null.
MessageType GetStatusInfo(Profile* profile,
                          const ProfileSyncService* service,
                          const SigninManagerBase& signin,
                          StatusLabelStyle style,
                          base::string16* status_label,
                          base::string16* link_label,
                          ActionType* action_type) {
  DCHECK_EQ(status_label == nullptr, link_label == nullptr);

  MessageType result_type(SYNCED);

  if (!signin.IsAuthenticated())
    return PRE_SYNCED;

  if (!service || service->IsFirstSetupComplete() ||
      service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
      service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_USER_CHOICE)) {
    // The order or priority is going to be: 1. Unrecoverable errors.
    // 2. Auth errors. 3. Protocol errors. 4. Passphrase errors.

    if (service && service->HasUnrecoverableError()) {
      if (status_label && link_label) {
        GetStatusForUnrecoverableError(profile, service, status_label,
                                       link_label, action_type);
      }
      return SYNC_ERROR;
    }

    // For auth errors first check if an auth is in progress.
    if (signin.AuthInProgress()) {
      if (status_label) {
        status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_AUTHENTICATING_LABEL));
      }
      return PRE_SYNCED;
    }

    PrefService* pref_service = profile->GetPrefs();
    syncer::SyncPrefs sync_prefs(pref_service);
    bool sync_everything = sync_prefs.HasKeepEverythingSynced();

    // Check for sync errors if the sync service is enabled.
    if (service) {
      // Since there is no auth in progress, check for an auth error first.
      GoogleServiceAuthError auth_error =
          SigninErrorControllerFactory::GetForProfile(profile)->auth_error();
      if (auth_error.state() != GoogleServiceAuthError::NONE) {
        if (status_label && link_label) {
          GetStatusForAuthError(profile, signin, status_label, link_label,
                                action_type);
        }
        return SYNC_ERROR;
      }

      // We don't have an auth error. Check for an actionable error.
      syncer::SyncStatus status;
      service->QueryDetailedSyncStatus(&status);
      if (status_label && link_label) {
        GetStatusForActionableError(status.sync_protocol_error, status_label,
                                    link_label, action_type);
        if (!status_label->empty())
          return SYNC_ERROR;
      }

      // Check for a passphrase error.
      if (service->IsPassphraseRequired() &&
          service->IsPassphraseRequiredForDecryption()) {
        if (status_label && link_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_STATUS_NEEDS_PASSWORD));
          link_label->assign(
              l10n_util::GetStringUTF16(
                  IDS_SYNC_STATUS_NEEDS_PASSWORD_LINK_LABEL));
          *action_type = ENTER_PASSPHRASE;
        }
        return SYNC_ERROR;
      }

      // Check to see if sync has been disabled via the dasboard and needs to be
      // set up once again.
      if (service->HasDisableReason(
              syncer::SyncService::DISABLE_REASON_USER_CHOICE) &&
          status.sync_protocol_error.error_type == syncer::NOT_MY_BIRTHDAY) {
        if (status_label) {
          status_label->assign(GetSyncedStateStatusLabel(service, signin, style,
                                                         sync_everything));
        }
        return PRE_SYNCED;
      }
    }

    // There is no error. Display "Last synced..." message.
    if (status_label) {
      status_label->assign(
          GetSyncedStateStatusLabel(service, signin, style, sync_everything));
    }
    return SYNCED;
  } else {
    // Either show auth error information with a link to re-login, auth in prog,
    // or provide a link to continue with setup.
    if (service->IsFirstSetupInProgress()) {
      result_type = PRE_SYNCED;
      syncer::SyncStatus status;
      service->QueryDetailedSyncStatus(&status);
      GoogleServiceAuthError auth_error =
          SigninErrorControllerFactory::GetForProfile(profile)->auth_error();
      if (status_label) {
        status_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_NTP_SETUP_IN_PROGRESS));
      }
      if (signin.AuthInProgress()) {
        if (status_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_AUTHENTICATING_LABEL));
        }
      } else if (auth_error.state() != GoogleServiceAuthError::NONE &&
                 auth_error.state() != GoogleServiceAuthError::TWO_FACTOR) {
        if (status_label && link_label) {
          GetStatusForAuthError(profile, signin, status_label, link_label,
                                action_type);
        }
        result_type = SYNC_ERROR;
      }
    } else if (service->HasUnrecoverableError()) {
      result_type = SYNC_ERROR;
      if (status_label && link_label) {
        GetStatusForUnrecoverableError(profile, service, status_label,
                                       link_label, action_type);
      }
    } else if (signin.IsAuthenticated()) {
      if (service->IsSyncConfirmationNeeded()) {
        if (status_label && link_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_SETTINGS_NOT_CONFIRMED));
          link_label->assign(l10n_util::GetStringUTF16(
              IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON));
        }
        *action_type = CONFIRM_SYNC_SETTINGS;
        result_type = SYNC_ERROR;
      } else {
        // The user is signed in, but sync has been stopped.
        result_type = PRE_SYNCED;
        if (status_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SIGNED_IN_WITH_SYNC_SUPPRESSED));
        }
      }
    }
  }
  return result_type;
}

}  // namespace

MessageType GetStatusLabels(Profile* profile,
                            const ProfileSyncService* service,
                            const SigninManagerBase& signin,
                            base::string16* status_label,
                            base::string16* link_label,
                            ActionType* action_type) {
  DCHECK(status_label);
  DCHECK(link_label);
  return GetStatusInfo(profile, service, signin, PLAIN_TEXT, status_label,
                       link_label, action_type);
}

#if !defined(OS_CHROMEOS)
AvatarSyncErrorType GetMessagesForAvatarSyncError(
    Profile* profile,
    const SigninManagerBase& signin,
    int* content_string_id,
    int* button_string_id) {
  const ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  // The order or priority is going to be: 1. Unrecoverable errors.
  // 2. Auth errors. 3. Protocol errors. 4. Passphrase errors.
  if (service && service->HasUnrecoverableError()) {
    // An unrecoverable error is sometimes accompanied by an actionable error.
    // If an actionable error is not set to be UPGRADE_CLIENT, then show a
    // generic unrecoverable error message.
    syncer::SyncStatus status;
    service->QueryDetailedSyncStatus(&status);
    if (status.sync_protocol_error.action != syncer::UPGRADE_CLIENT) {
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
  }

  // Check for an auth error.
  SigninErrorController* signin_error_controller =
      SigninErrorControllerFactory::GetForProfile(profile);
  if (signin_error_controller && signin_error_controller->HasError()) {
    if (profile->IsSupervised()) {
      // For a supervised user, no direct action can be taken to resolve an
      // auth token error.
      *content_string_id = IDS_SYNC_ERROR_USER_MENU_SUPERVISED_SIGNIN_MESSAGE;
      *button_string_id = 0;
      return SUPERVISED_USER_AUTH_ERROR;
    }
    // For a non-supervised user, the user can reauth to resolve the signin
    // error.
    *content_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_MESSAGE;
    *button_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_BUTTON;
    return AUTH_ERROR;
  }

  // Check for sync errors if the sync service is enabled.
  if (service) {
    // Check for an actionable UPGRADE_CLIENT error.
    syncer::SyncStatus status;
    service->QueryDetailedSyncStatus(&status);
    if (status.sync_protocol_error.action == syncer::UPGRADE_CLIENT) {
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
    if (signin.IsAuthenticated() && service->IsSyncConfirmationNeeded()) {
      *content_string_id = IDS_SYNC_SETTINGS_NOT_CONFIRMED;
      *button_string_id = IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON;
      return SETTINGS_UNCONFIRMED_ERROR;
    }
  }

  // There is no error.
  return NO_SYNC_ERROR;
}
#endif

MessageType GetStatus(Profile* profile,
                      const ProfileSyncService* service,
                      const SigninManagerBase& signin) {
  ActionType action_type = NO_ACTION;
  return GetStatusInfo(profile, service, signin, WITH_HTML, nullptr, nullptr,
                       &action_type);
}

bool ShouldShowPassphraseError(const ProfileSyncService* service) {
  return service->IsFirstSetupComplete() && service->IsPassphraseRequired() &&
         service->IsPassphraseRequiredForDecryption();
}

}  // namespace sync_ui_util
