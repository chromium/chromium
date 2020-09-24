// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/signin_error_notifier_ash.h"

#include <memory>

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/account_manager/account_manager_util.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/account_manager/account_manager_welcome_dialog.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace {

constexpr char kProfileSigninNotificationId[] = "chrome://settings/signin/";
constexpr char kSecondaryAccountNotificationIdSuffix[] = "/secondary-account";

bool g_ignore_sync_errors_for_test_ = false;

void HandleDeviceAccountReauthNotificationClick(
    base::Optional<int> button_index) {
  chrome::AttemptUserExit();
}

bool AreAllAccountsMigrated(
    const chromeos::AccountManager* const account_manager,
    const std::vector<chromeos::AccountManager::Account>& accounts) {
  for (const auto& account : accounts) {
    if (account_manager->HasDummyGaiaToken(account.key)) {
      return false;
    }
  }
  return true;
}

// Returns true if the child user has migrated at least one of their
// secondary edu accounts to ARC++.
bool IsSecondaryEduAccountMigratedForChildUser(
    Profile* profile,
    const std::vector<chromeos::AccountManager::Account>& accounts) {
  // If the profile is not a child then there is no migration required.
  // If the profile is child but has only one account on device, then there is
  // no migration required; i.e. there is no secondary edu account to migrate.
  if (!profile->IsChild() || accounts.size() < 2) {
    return true;
  }

  return profile->GetPrefs()->GetBoolean(
      prefs::kEduCoexistenceArcMigrationCompleted);
}

}  // namespace

SigninErrorNotifier::SigninErrorNotifier(SigninErrorController* controller,
                                         Profile* profile)
    : error_controller_(controller),
      profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile_)),
      account_manager_(g_browser_process->platform_part()
                           ->GetAccountManagerFactory()
                           ->GetAccountManager(profile_->GetPath().value())) {
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
    token_handle_util_ = std::make_unique<TokenHandleUtil>();
    token_handle_util_->CheckToken(
        account_id, profile->GetURLLoaderFactory(),
        base::Bind(&SigninErrorNotifier::OnTokenHandleCheck,
                   weak_factory_.GetWeakPtr()));
  }
  OnErrorChanged();
}

void SigninErrorNotifier::OnTokenHandleCheck(
    const AccountId& account_id,
    TokenHandleUtil::TokenHandleStatus status) {
  if (status != TokenHandleUtil::INVALID)
    return;
  RecordReauthReason(account_id, chromeos::ReauthReason::INVALID_TOKEN_HANDLE);
  HandleDeviceAccountError();
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
    NotificationDisplayService::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, device_account_notification_id_);
    NotificationDisplayService::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT,
        secondary_account_notification_id_);
    return;
  }

  if (user_manager::UserManager::IsInitialized()) {
    chromeos::UserFlow* user_flow =
        chromeos::ChromeUserManager::Get()->GetCurrentUserFlow();

    // Check whether Chrome OS user flow allows launching browser.
    // Example: Supervised user creation flow which handles token invalidation
    // itself and notifications should be suppressed. http://crbug.com/359045
    if (!user_flow->ShouldLaunchBrowser())
      return;
  }

  const AccountId account_id =
      multi_user_util::GetAccountIdFromProfile(profile_);
  if (!chromeos::IsAccountManagerAvailable(profile_)) {
    // If this flag is disabled, Chrome OS does not have a concept of Secondary
    // Accounts. Preserve existing behavior.
    RecordReauthReason(account_id, chromeos::ReauthReason::SYNC_FAILED);
    HandleDeviceAccountError();
    return;
  }

  const CoreAccountId error_account_id = error_controller_->error_account_id();
  const CoreAccountId primary_account_id =
      identity_manager_->GetPrimaryAccountId(
          signin::ConsentLevel::kNotRequired);
  if (error_account_id == primary_account_id) {
    RecordReauthReason(account_id, chromeos::ReauthReason::SYNC_FAILED);
    HandleDeviceAccountError();
  } else {
    HandleSecondaryAccountError(error_account_id);
  }
}

void SigninErrorNotifier::HandleDeviceAccountError() {
  // If this error has occurred because a user's account has just been converted
  // to a Family Link Supervised account, then suppress the notificaiton.
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  if (service->signout_required_after_supervision_enabled())
    return;

  const AccountId account_id =
      multi_user_util::GetAccountIdFromProfile(profile_);
  // We need to save the flag in the local state because
  // TokenHandleUtil::CheckToken might fail on the login screen due to lack of
  // network connectivity.
  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      account_id, true /* force_online_signin */);

  // Add an accept button to sign the user out.
  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_BUTTON)));

  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kProfileSigninNotificationId);

  // Set |profile_id| for multi-user notification blocker.
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile_).GetUserEmail();

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          device_account_notification_id_,
          l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_BUBBLE_VIEW_TITLE),
          GetMessageBody(false /* is_secondary_account_error */),
          l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_DISPLAY_SOURCE),
          GURL(device_account_notification_id_), notifier_id, data,
          new message_center::HandleNotificationClickDelegate(
              base::BindRepeating(&HandleDeviceAccountReauthNotificationClick)),
          chromeos::kNotificationWarningIcon,
          message_center::SystemNotificationWarningLevel::WARNING);
  notification->SetSystemPriority();

  // Update or add the notification.
  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void SigninErrorNotifier::HandleSecondaryAccountError(
    const CoreAccountId& account_id) {
  account_manager_->GetAccounts(base::BindOnce(
      &SigninErrorNotifier::OnGetAccounts, weak_factory_.GetWeakPtr()));
}

void SigninErrorNotifier::OnGetAccounts(
    const std::vector<chromeos::AccountManager::Account>& accounts) {
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kProfileSigninNotificationId);
  // Set |profile_id| for multi-user notification blocker. Note the primary user
  // account id is used to identify the profile for the blocker so it is used
  // instead of the secondary user account id.
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile_).GetUserEmail();

  const bool are_all_accounts_migrated =
      AreAllAccountsMigrated(account_manager_, accounts) &&
      IsSecondaryEduAccountMigratedForChildUser(profile_, accounts);

  const base::string16 message_title =
      are_all_accounts_migrated
          ? l10n_util::GetStringUTF16(
                IDS_SIGNIN_ERROR_SECONDARY_ACCOUNT_BUBBLE_VIEW_TITLE)
          : l10n_util::GetStringUTF16(
                IDS_SIGNIN_ERROR_SECONDARY_ACCOUNT_MIGRATION_BUBBLE_VIEW_TITLE);
  const base::string16 message_body =
      are_all_accounts_migrated
          ? GetMessageBody(true /* is_secondary_account_error */)
          : l10n_util::GetStringUTF16(
                IDS_SIGNIN_ERROR_SECONDARY_ACCOUNT_MIGRATION_BUBBLE_VIEW_MESSAGE);

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          secondary_account_notification_id_, message_title, message_body,
          l10n_util::GetStringUTF16(
              IDS_SIGNIN_ERROR_SECONDARY_ACCOUNT_DISPLAY_SOURCE),
          GURL(secondary_account_notification_id_), notifier_id,
          message_center::RichNotificationData(),
          new message_center::HandleNotificationClickDelegate(
              base::BindRepeating(
                  &SigninErrorNotifier::
                      HandleSecondaryAccountReauthNotificationClick,
                  weak_factory_.GetWeakPtr())),
          vector_icons::kSettingsIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->SetSystemPriority();

  // Update or add the notification.
  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void SigninErrorNotifier::HandleSecondaryAccountReauthNotificationClick(
    base::Optional<int> button_index) {
  if (profile_->IsChild() && !profile_->GetPrefs()->GetBoolean(
                                 prefs::kEduCoexistenceArcMigrationCompleted)) {
    if (!chromeos::AccountManagerWelcomeDialog::
            ShowIfRequiredForEduCoexistence()) {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile_, chromeos::settings::mojom::kMyAccountsSubpagePath);
    }
    return;
  }

  if (!chromeos::AccountManagerWelcomeDialog::ShowIfRequired()) {
    // The welcome dialog was not shown (because it has been shown too many
    // times already). Take users to Account Manager UI directly.
    // Note: If the welcome dialog was shown, we don't need to do anything.
    // Closing that dialog takes users to Account Manager UI.

    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile_, chromeos::settings::mojom::kMyAccountsSubpagePath);
  }
}

base::string16 SigninErrorNotifier::GetMessageBody(
    bool is_secondary_account_error) const {
  if (is_secondary_account_error) {
    return l10n_util::GetStringUTF16(
        IDS_SIGNIN_ERROR_SECONDARY_ACCOUNT_BUBBLE_VIEW_MESSAGE);
  }

  switch (error_controller_->auth_error().state()) {
    // TODO(rogerta): use account id in error messages.

    // User credentials are invalid (bad acct, etc).
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::SERVICE_ERROR:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE);
      break;

    // Sync service is not available for this account's domain.
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_UNAVAILABLE_ERROR_BUBBLE_VIEW_MESSAGE);
      break;

    // Generic message for "other" errors.
    default:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_OTHER_SIGN_IN_ERROR_BUBBLE_VIEW_MESSAGE);
  }
}
