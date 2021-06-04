// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/user_board_view_mojo.h"

#include <utility>

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "chrome/browser/ash/login/lock_screen_utils.h"
#include "chrome/browser/ui/ash/login_screen_client_impl.h"

namespace chromeos {

namespace {

ash::EasyUnlockIconState GetEasyUnlockIconStateFromUserPodCustomIconId(
    proximity_auth::ScreenlockBridge::UserPodCustomIcon icon) {
  switch (icon) {
    case proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_NONE:
      return ash::EasyUnlockIconState::NONE;
    case proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_HARDLOCKED:
      return ash::EasyUnlockIconState::HARDLOCKED;
    case proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_LOCKED:
      return ash::EasyUnlockIconState::LOCKED;
    case proximity_auth::ScreenlockBridge::
        USER_POD_CUSTOM_ICON_LOCKED_TO_BE_ACTIVATED:
      return ash::EasyUnlockIconState::LOCKED_TO_BE_ACTIVATED;
    case proximity_auth::ScreenlockBridge::
        USER_POD_CUSTOM_ICON_LOCKED_WITH_PROXIMITY_HINT:
      return ash::EasyUnlockIconState::LOCKED_WITH_PROXIMITY_HINT;
    case proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_UNLOCKED:
      return ash::EasyUnlockIconState::UNLOCKED;
    case proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_SPINNER:
      return ash::EasyUnlockIconState::SPINNER;
  }
}

// Converts parameters to a mojo struct that can be sent to the
// screenlock view-based UI.
ash::EasyUnlockIconInfo ToEasyUnlockIconInfo(
    const proximity_auth::ScreenlockBridge::UserPodCustomIconInfo&
        user_pod_icon_info) {
  ash::EasyUnlockIconInfo easy_unlock_icon_info;
  easy_unlock_icon_info.icon_state =
      GetEasyUnlockIconStateFromUserPodCustomIconId(user_pod_icon_info.icon());

  if (!user_pod_icon_info.tooltip().empty()) {
    easy_unlock_icon_info.tooltip = user_pod_icon_info.tooltip();
    easy_unlock_icon_info.autoshow_tooltip =
        user_pod_icon_info.autoshow_tooltip();
  }

  if (!user_pod_icon_info.aria_label().empty())
    easy_unlock_icon_info.aria_label = user_pod_icon_info.aria_label();

  if (user_pod_icon_info.hardlock_on_click())
    easy_unlock_icon_info.hardlock_on_click = true;

  return easy_unlock_icon_info;
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

  // Send a request to get keyboard layouts for `default_locale`.
  LoginScreenClientImpl::Get()->RequestPublicSessionKeyboardLayouts(
      account_id, default_locale);
}

void UserBoardViewMojo::SetPublicSessionShowFullManagementDisclosure(
    bool show_full_management_disclosure) {
  ash::LoginScreen::Get()
      ->GetModel()
      ->SetPublicSessionShowFullManagementDisclosure(
          show_full_management_disclosure);
}

void UserBoardViewMojo::ShowBannerMessage(const std::u16string& message,
                                          bool is_warning) {
  // As of M69, ShowBannerMessage is used only for showing ext4 migration
  // warning banner message.
  // TODO(fukino): Remove ShowWarningMessage and related implementation along
  // with the migration screen once the transition to ext4 is compilete.
  ash::LoginScreen::Get()->GetModel()->UpdateWarningMessage(message);
}

void UserBoardViewMojo::ShowUserPodCustomIcon(
    const AccountId& account_id,
    const proximity_auth::ScreenlockBridge::UserPodCustomIconInfo& icon_info) {
  ash::LoginScreen::Get()->GetModel()->ShowEasyUnlockIcon(
      account_id, ToEasyUnlockIconInfo(icon_info));
}

void UserBoardViewMojo::HideUserPodCustomIcon(const AccountId& account_id) {
  ash::LoginScreen::Get()->GetModel()->ShowEasyUnlockIcon(account_id, {});
}

void UserBoardViewMojo::SetAuthType(const AccountId& account_id,
                                    proximity_auth::mojom::AuthType auth_type,
                                    const std::u16string& initial_value) {
  ash::LoginScreen::Get()->GetModel()->SetTapToUnlockEnabledForUser(
      account_id, auth_type == proximity_auth::mojom::AuthType::USER_CLICK);

  if (auth_type == proximity_auth::mojom::AuthType::ONLINE_SIGN_IN) {
    ash::LoginScreen::Get()->GetModel()->ForceOnlineSignInForUser(account_id);
  }
}

void UserBoardViewMojo::SetTpmLockedState(const AccountId& account_id,
                                          bool is_locked,
                                          base::TimeDelta time_left) {
  ash::LoginScreen::Get()->GetModel()->SetTpmLockedState(account_id, is_locked,
                                                         time_left);
}

base::WeakPtr<UserBoardView> UserBoardViewMojo::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace chromeos
