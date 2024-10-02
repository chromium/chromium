// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/signin_error_notifier.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/signin/token_handle_fetcher.h"
#include "chrome/browser/ash/login/signin/token_handle_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/user_manager/user_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {
namespace {

constexpr char kProfileSigninNotificationId[] = "chrome://settings/signin/";
constexpr char kSecondaryAccountNotificationIdSuffix[] = "/secondary-account";

bool g_ignore_sync_errors_for_test_ = false;

void HandleDeviceAccountReauthNotificationClick(
    std::optional<int> button_index) {
  chrome::AttemptUserExit();
}

bool AreAllAccountsMigrated(
    const std::vector<std::pair<account_manager::Account, bool>>&
        account_dummy_token_list) {
  for (const auto& account : account_dummy_token_list) {
    if (account.second) {
      // Account has a dummy Gaia token.
      return false;
    }
  }
  return true;
}

// Returns true if the child user has migrated at least one of their
// secondary edu accounts to ARC++.
bool IsSecondaryEduAccountMigratedForChildUser(Profile* profile,
                                               int accounts_size) {
  // If the profile is not a child then there is no migration required.
  // If the profile is child but has only one account on device, then there is
  // no migration required; i.e. there is no secondary edu account to migrate.
  if (!profile->IsChild() || accounts_size < 2) {
    return true;
  }

  return profile->GetPrefs()->GetBoolean(
      prefs::kEduCoexistenceArcMigrationCompleted);
}

std::unique_ptr<message_center::Notification>
CreateDeviceAccountErrorNotification(
    const std::string& email,
    const std::string& device_account_notification_id,
    const std::u16string& error_message) {
  // Add an accept button to sign the user out.
  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_BUTTON)));

  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kProfileSigninNotificationId,
      NotificationCatalogName::kDeviceAccountSigninError);

  // Set `profile_id` for multi-user notification blocker.
  notifier_id.profile_id = email;

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          device_account_notification_id,
          l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_BUBBLE_VIEW_TITLE),
          error_message,
          l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_DISPLAY_SOURCE),
          GURL(device_account_notification_id), notifier_id, data,
          new message_center::HandleNotificationClickDelegate(
              base::BindRepeating(&HandleDeviceAccountReauthNotificationClick)),
          vector_icons::kNotificationWarningIcon,
          message_center::SystemNotificationWarningLevel::WARNING);
  notification->SetSystemPriority();

  return notification;
}

void SaveForceOnlineSignin(const Profile* const profile) {
  const AccountId account_id =
      multi_user_util::GetAccountIdFromProfile(profile);
  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      account_id, true /* force_online_signin */);
}

std::u16string GetMessageBodyForSecondaryAccountErrors() {
  return l10n_util::GetStringUTF16(
      IDS_SIGNIN_ERROR_SECONDARY_ACCOUNT_BUBBLE_VIEW_MESSAGE);
}

std::u16string GetMessageBodyForDeviceAccountErrors(
    const GoogleServiceAuthError::State& error_state) {
  switch (error_state) {
    // User credentials are invalid (bad acct, etc).
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::SERVICE_ERROR:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE);

    // Sync service is not available for this account's domain.
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_UNAVAILABLE_ERROR_BUBBLE_VIEW_MESSAGE);

    // Generic message for "other" errors.
    default:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_OTHER_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE);
  }
}

std::unique_ptr<TokenHandleFetcher> CreateTokenHandleFetcher(
    Profile* profile,
    TokenHandleUtil* token_handle_util) {
  const AccountId account_id =
      multi_user_util::GetAccountIdFromProfile(profile);
  return std::make_unique<TokenHandleFetcher>(profile, token_handle_util,
                                              account_id);
}

}  // namespace

SigninErrorNotifier::SigninErrorNotifier(SigninErrorController* controller,
                                         Profile* profile)
    : error_controller_(controller),
      profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile_)),
      account_manager_(g_browser_process->platform_part()
                           ->GetAccountManagerFactory()
                           ->GetAccountManager(profile_->GetPath().value())),
      token_handle_util_(std::make_unique<TokenHandleUtil>()),
      token_handle_fetcher_(
          CreateTokenHandleFetcher(profile_, token_handle_util_.get())) {
  DCHECK(account_manager_);
  // Create a unique notification ID for this profile.
  device_account_notification_id_ =
      kProfileSigninNotificationId + profile->GetProfileUserName();
  secondary_account_notification_id_ =
      device_account_notification_id_ + kSecondaryAccountNotificationIdSuffix;

  error_controller_->AddObserver(this);
  const AccountId account_id =
      multi_user_util::GetAccountIdFromProfile(profile_);
  if (TokenHandleUtil::HasToken(account_id) &&
      !TokenHandleUtil::IsRecentlyChecked(account_id)) {
    token_handle_util_->IsReauthRequired(
        account_id, profile->GetURLLoaderFactory(),
        base::BindOnce(&SigninErrorNotifier::OnTokenHandleCheck,
                       weak_factory_.GetWeakPtr()));
  }
  OnErrorChanged();
}

void SigninErrorNotifier::OnTokenHandleCheck(const AccountId& account_id,
                                             const std::string& token,
                                             bool reauth_required) {
  token_handle_fetcher_->DiagnoseTokenHandleMapping(account_id, token);
  if (!reauth_required) {
    return;
  }
  RecordReauthReason(account_id, ReauthReason::kInvalidTokenHandle);
  HandleDeviceAccountError(/*error_message=*/l10n_util::GetStringUTF16(
      IDS_SYNC_TOKEN_HANDLE_ERROR_BUBBLE_VIEW_MESSAGE));
}

SigninErrorNotifier::~SigninErrorNotifier() {
  DCHECK(!error_controller_)
      << "SigninErrorNotifier::Shutdown() was not called";
}

// static
std::unique_ptr<base::AutoReset<bool>>
SigninErrorNotifier::IgnoreSyncErrorsForTesting() {
  return std::make_unique<base::AutoReset<bool>>(
      &g_ignore_sync_errors_for_test_, true);
}

// static
bool SigninErrorNotifier::ShouldIgnoreSyncErrorsForTesting() {
  return g_ignore_sync_errors_for_test_;
}

// static
void SigninErrorNotifier::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kEduCoexistenceArcMigrationCompleted,
                                false);
}

void SigninErrorNotifier::Shutdown() {
  error_controller_->RemoveObserver(this);
  error_controller_ = nullptr;
}

void SigninErrorNotifier::OnErrorChanged() {
  if (g_ignore_sync_errors_for_test_)
    return;

  if (!error_controller_->HasError()) {
    NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, device_account_notification_id_);
    NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT,
        secondary_account_notification_id_);
    return;
  }

  const AccountId account_id =
      multi_user_util::GetAccountIdFromProfile(profile_);
  if (!IsAccountManagerAvailable(profile_)) {
    // If this flag is disabled, Chrome OS does not have a concept of Secondary
    // Accounts. Preserve existing behavior.
    RecordReauthReason(account_id, ReauthReason::kSyncFailed);
    HandleDeviceAccountError(
        /*error_message=*/GetMessageBodyForDeviceAccountErrors(
            /*error=*/error_controller_->auth_error().state()));
    return;
  }

  const CoreAccountId error_account_id = error_controller_->error_account_id();
  const CoreAccountId primary_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (error_account_id == primary_account_id) {
    RecordReauthReason(account_id, ReauthReason::kSyncFailed);
    HandleDeviceAccountError(
        /*error_message=*/GetMessageBodyForDeviceAccountErrors(
            /*error=*/error_controller_->auth_error().state()));
  } else {
    HandleSecondaryAccountError(error_account_id);
  }
}

void SigninErrorNotifier::HandleDeviceAccountError(
    const std::u16string& error_message) {
  // If this error has occurred because a user's account has just been converted
  // to a Family Link Supervised account, then suppress the notification.
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  if (service->signout_required_after_supervision_enabled())
    return;

  // We need to save the flag in the local state because
  // TokenHandleUtil::IsReauthRequired might fail on the login screen due to
  // lack of network connectivity.
  SaveForceOnlineSignin(profile_);
  std::unique_ptr<message_center::Notification> notification =
      CreateDeviceAccountErrorNotification(
          /*email=*/multi_user_util::GetAccountIdFromProfile(profile_)
              .GetUserEmail(),
          device_account_notification_id_, error_message);

  // Update or add the notification.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void SigninErrorNotifier::HandleSecondaryAccountError(
    const CoreAccountId& account_id) {
  account_manager_->CheckDummyGaiaTokenForAllAccounts(
      base::BindOnce(&SigninErrorNotifier::OnCheckDummyGaiaTokenForAllAccounts,
                     weak_factory_.GetWeakPtr()));
}

void SigninErrorNotifier::OnCheckDummyGaiaTokenForAllAccounts(
    const std::vector<std::pair<account_manager::Account, bool>>&
        account_dummy_token_list) {
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kProfileSigninNotificationId,
      NotificationCatalogName::kSecondaryAccountSigninError);
  // Set `profile_id` for multi-user notification blocker. Note the primary user
  // account id is used to identify the profile for the blocker so it is used
  // instead of the secondary user account id.
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile_).GetUserEmail();

  const bool are_all_accounts_migrated =
      AreAllAccountsMigrated(account_dummy_token_list) &&
      IsSecondaryEduAccountMigratedForChildUser(
          profile_, account_dummy_token_list.size());

  const std::u16string message_title =
      are_all_accounts_migrated
          ? l10n_util::GetStringUTF16(
                IDS_SIGNIN_ERROR_SECONDARY_ACCOUNT_BUBBLE_VIEW_TITLE)
          : l10n_util::GetStringUTF16(
                IDS_SIGNIN_ERROR_SECONDARY_ACCOUNT_MIGRATION_BUBBLE_VIEW_TITLE);
  const std::u16string message_body =
      are_all_accounts_migrated
          ? GetMessageBodyForSecondaryAccountErrors()
          : l10n_util::GetStringUTF16(
                IDS_SIGNIN_ERROR_SECONDARY_ACCOUNT_MIGRATION_BUBBLE_VIEW_MESSAGE);

  message_center::Notification notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      secondary_account_notification_id_, message_title, message_body,
      l10n_util::GetStringUTF16(
          IDS_SIGNIN_ERROR_SECONDARY_ACCOUNT_DISPLAY_SOURCE),
      GURL(secondary_account_notification_id_), notifier_id,
      message_center::RichNotificationData(),
      new message_center::HandleNotificationClickDelegate(base::BindRepeating(
          &SigninErrorNotifier::HandleSecondaryAccountReauthNotificationClick,
          weak_factory_.GetWeakPtr())),
      vector_icons::kSettingsIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification.SetSystemPriority();

  // Update or add the notification.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, notification,
      /*metadata=*/nullptr);
}

void SigninErrorNotifier::HandleSecondaryAccountReauthNotificationClick(
    std::optional<int> button_index) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, ash::features::IsOsSettingsRevampWayfindingEnabled()
                    ? chromeos::settings::mojom::kPeopleSectionPath
                    : chromeos::settings::mojom::kMyAccountsSubpagePath);
}

}  // namespace ash
