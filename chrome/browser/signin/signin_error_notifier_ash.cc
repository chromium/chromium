// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_error_notifier_ash.h"

#include <memory>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/account_manager/account_manager_util.h"
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
#include "chrome/browser/ui/webui/chromeos/account_manager_welcome_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "components/account_id/account_id.h"
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
  OnErrorChanged();
}

SigninErrorNotifier::~SigninErrorNotifier() {
  DCHECK(!error_controller_)
      << "SigninErrorNotifier::Shutdown() was not called";
}

void SigninErrorNotifier::Shutdown() {
  error_controller_->RemoveObserver(this);
  error_controller_ = NULL;
}

void SigninErrorNotifier::OnErrorChanged() {
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

  if (!chromeos::IsAccountManagerAvailable(profile_)) {
    // If this flag is disabled, Chrome OS does not have a concept of Secondary
    // Accounts. Preserve existing behavior.
    HandleDeviceAccountError();
    return;
  }

  const std::string error_account_id = error_controller_->error_account_id();
  if (error_account_id ==
      identity_manager_->GetPrimaryAccountInfo().account_id) {
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

  // Add an accept button to sign the user out.
  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL)));

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
          ash::kNotificationWarningIcon,
          message_center::SystemNotificationWarningLevel::WARNING);
  notification->SetSystemPriority();

  // Update or add the notification.
  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void SigninErrorNotifier::HandleSecondaryAccountError(
    const std::string& account_id) {
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
      AreAllAccountsMigrated(account_manager_, accounts);
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
  if (!chromeos::AccountManagerWelcomeDialog::ShowIfRequired()) {
    // The welcome dialog was not shown (because it has been shown too many
    // times already). Take users to Account Manager UI directly.
    // Note: If the welcome dialog was shown, we don't need to do anything.
    // Closing that dialog takes users to Account Manager UI.

    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile_, chrome::kAccountManagerSubPage);
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
