// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/user_board_view_mojo.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "chrome/browser/ash/login/lock_screen_utils.h"
#include "chrome/browser/ui/ash/login_screen_client_impl.h"

namespace ash {

UserBoardViewMojo::UserBoardViewMojo() {}

UserBoardViewMojo::~UserBoardViewMojo() = default;

void UserBoardViewMojo::SetPublicSessionDisplayName(
    const AccountId& account_id,
    const std::string& display_name) {
  LoginScreen::Get()->GetModel()->SetPublicSessionDisplayName(account_id,
                                                              display_name);
}

void UserBoardViewMojo::SetPublicSessionLocales(
    const AccountId& account_id,
    base::Value::List locales,
    const std::string& default_locale,
    bool multiple_recommended_locales) {
  LoginScreen::Get()->GetModel()->SetPublicSessionLocales(
      account_id,
      lock_screen_utils::FromListValueToLocaleItem(std::move(locales)),
      default_locale, multiple_recommended_locales);

  // Send a request to get keyboard layouts for `default_locale`.
  LoginScreenClientImpl::Get()->RequestPublicSessionKeyboardLayouts(
      account_id, default_locale);
}

void UserBoardViewMojo::SetPublicSessionShowFullManagementDisclosure(
    bool show_full_management_disclosure) {
  LoginScreen::Get()->GetModel()->SetPublicSessionShowFullManagementDisclosure(
      show_full_management_disclosure);
}

void UserBoardViewMojo::ShowBannerMessage(const std::u16string& message,
                                          bool is_warning) {
  // As of M69, ShowBannerMessage is used only for showing ext4 migration
  // warning banner message.
  // TODO(fukino): Remove ShowWarningMessage and related implementation along
  // with the migration screen once the transition to ext4 is compilete.
  LoginScreen::Get()->GetModel()->UpdateWarningMessage(message);
}

void UserBoardViewMojo::SetSmartLockState(const AccountId& account_id,
                                          SmartLockState state) {
  if (base::FeatureList::IsEnabled(ash::features::kSmartLockUIRevamp)) {
    LoginScreen::Get()->GetModel()->SetSmartLockState(account_id, state);
  }
}

void UserBoardViewMojo::NotifySmartLockAuthResult(const AccountId& account_id,
                                                  bool success) {
  if (base::FeatureList::IsEnabled(ash::features::kSmartLockUIRevamp)) {
    LoginScreen::Get()->GetModel()->NotifySmartLockAuthResult(account_id,
                                                              success);
  }
}

void UserBoardViewMojo::SetAuthType(const AccountId& account_id,
                                    proximity_auth::mojom::AuthType auth_type,
                                    const std::u16string& initial_value) {
  LoginScreen::Get()->GetModel()->SetTapToUnlockEnabledForUser(
      account_id, auth_type == proximity_auth::mojom::AuthType::USER_CLICK);

  if (auth_type == proximity_auth::mojom::AuthType::ONLINE_SIGN_IN) {
    LoginScreen::Get()->GetModel()->ForceOnlineSignInForUser(account_id);
  }
}

void UserBoardViewMojo::SetTpmLockedState(const AccountId& account_id,
                                          bool is_locked,
                                          base::TimeDelta time_left) {
  LoginScreen::Get()->GetModel()->SetTpmLockedState(account_id, is_locked,
                                                    time_left);
}

base::WeakPtr<UserBoardView> UserBoardViewMojo::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash
