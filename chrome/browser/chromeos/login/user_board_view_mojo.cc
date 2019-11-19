// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_board_view_mojo.h"

#include <utility>

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "chrome/browser/chromeos/login/lock_screen_utils.h"
#include "chrome/browser/ui/ash/login_screen_client.h"

namespace chromeos {

namespace {

ash::EasyUnlockIconId GetEasyUnlockIconIdFromUserPodCustomIconId(
    proximity_auth::ScreenlockBridge::UserPodCustomIcon icon) {
  switch (icon) {
    case proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_NONE:
      return ash::EasyUnlockIconId::NONE;
    case proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_HARDLOCKED:
      return ash::EasyUnlockIconId::HARDLOCKED;
    case proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_LOCKED:
      return ash::EasyUnlockIconId::LOCKED;
    case proximity_auth::ScreenlockBridge::
        USER_POD_CUSTOM_ICON_LOCKED_TO_BE_ACTIVATED:
      return ash::EasyUnlockIconId::LOCKED_TO_BE_ACTIVATED;
    case proximity_auth::ScreenlockBridge::
        USER_POD_CUSTOM_ICON_LOCKED_WITH_PROXIMITY_HINT:
      return ash::EasyUnlockIconId::LOCKED_WITH_PROXIMITY_HINT;
    case proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_UNLOCKED:
      return ash::EasyUnlockIconId::UNLOCKED;
    case proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_SPINNER:
      return ash::EasyUnlockIconId::SPINNER;
  }
}

// Converts parameters to a mojo struct that can be sent to the
// screenlock view-based UI.
ash::EasyUnlockIconOptions ToEasyUnlockIconOptions(
    const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions&
        icon_options) {
  ash::EasyUnlockIconOptions options;
  options.icon =
      GetEasyUnlockIconIdFromUserPodCustomIconId(icon_options.icon());

  if (!icon_options.tooltip().empty()) {
    options.tooltip = icon_options.tooltip();
    options.autoshow_tooltip = icon_options.autoshow_tooltip();
  }

  if (!icon_options.aria_label().empty())
    options.aria_label = icon_options.aria_label();

  if (icon_options.hardlock_on_click())
    options.hardlock_on_click = true;

  return options;
}

}  // namespace

UserBoardViewMojo::UserBoardViewMojo() {}

UserBoardViewMojo::~UserBoardViewMojo() = default;

void UserBoardViewMojo::SetPublicSessionDisplayName(
    const AccountId& account_id,
    const std::string& display_name) {
  ash::LoginScreen::Get()->GetModel()->SetPublicSessionDisplayName(
      account_id, display_name);
}

void UserBoardViewMojo::SetPublicSessionLocales(
    const AccountId& account_id,
    std::unique_ptr<base::ListValue> locales,
    const std::string& default_locale,
    bool multiple_recommended_locales) {
  DCHECK(locales);
  ash::LoginScreen::Get()->GetModel()->SetPublicSessionLocales(
      account_id,
      lock_screen_utils::FromListValueToLocaleItem(std::move(locales)),
      default_locale, multiple_recommended_locales);

  // Send a request to get keyboard layouts for |default_locale|.
  LoginScreenClient::Get()->RequestPublicSessionKeyboardLayouts(account_id,
                                                                default_locale);
}

void UserBoardViewMojo::SetPublicSessionShowFullManagementDisclosure(
    bool show_full_management_disclosure) {
  ash::LoginScreen::Get()
      ->GetModel()
      ->SetPublicSessionShowFullManagementDisclosure(
          show_full_management_disclosure);
}

void UserBoardViewMojo::ShowBannerMessage(const base::string16& message,
                                          bool is_warning) {
  // As of M69, ShowBannerMessage is used only for showing ext4 migration
  // warning banner message.
  // TODO(fukino): Remove ShowWarningMessage and related implementation along
  // with the migration screen once the transition to ext4 is compilete.
  ash::LoginScreen::Get()->GetModel()->UpdateWarningMessage(message);
}

void UserBoardViewMojo::ShowUserPodCustomIcon(
    const AccountId& account_id,
    const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions&
        icon_options) {
  ash::LoginScreen::Get()->GetModel()->ShowEasyUnlockIcon(
      account_id, ToEasyUnlockIconOptions(icon_options));
}

void UserBoardViewMojo::HideUserPodCustomIcon(const AccountId& account_id) {
  ash::LoginScreen::Get()->GetModel()->ShowEasyUnlockIcon(account_id, {});
}

void UserBoardViewMojo::SetAuthType(const AccountId& account_id,
                                    proximity_auth::mojom::AuthType auth_type,
                                    const base::string16& initial_value) {
  ash::LoginScreen::Get()->GetModel()->SetTapToUnlockEnabledForUser(
      account_id, auth_type == proximity_auth::mojom::AuthType::USER_CLICK);

  if (auth_type == proximity_auth::mojom::AuthType::ONLINE_SIGN_IN) {
    ash::LoginScreen::Get()->GetModel()->ForceOnlineSignInForUser(account_id);
  }
}

base::WeakPtr<UserBoardView> UserBoardViewMojo::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace chromeos
