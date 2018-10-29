// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_data_dispatcher.h"

namespace ash {

LoginDataDispatcher::Observer::~Observer() = default;

void LoginDataDispatcher::Observer::OnUsersChanged(
    const std::vector<mojom::LoginUserInfoPtr>& users) {}

void LoginDataDispatcher::Observer::OnPinEnabledForUserChanged(
    const AccountId& user,
    bool enabled) {}

void LoginDataDispatcher::Observer::OnFingerprintStateChanged(
    const AccountId& account_id,
    mojom::FingerprintState state) {}

void LoginDataDispatcher::Observer::OnFingerprintAuthResult(
    const AccountId& account_id,
    bool successful) {}

void LoginDataDispatcher::Observer::OnAuthEnabledForUserChanged(
    const AccountId& user,
    bool enabled,
    const base::Optional<base::Time>& auth_reenabled_time) {}

void LoginDataDispatcher::Observer::OnTapToUnlockEnabledForUserChanged(
    const AccountId& user,
    bool enabled) {}

void LoginDataDispatcher::Observer::OnForceOnlineSignInForUser(
    const AccountId& user) {}

void LoginDataDispatcher::Observer::OnLockScreenNoteStateChanged(
    mojom::TrayActionState state) {}

void LoginDataDispatcher::Observer::OnShowEasyUnlockIcon(
    const AccountId& user,
    const mojom::EasyUnlockIconOptionsPtr& icon) {}

void LoginDataDispatcher::Observer::OnShowWarningBanner(
    const base::string16& message) {}

void LoginDataDispatcher::Observer::OnHideWarningBanner() {}

void LoginDataDispatcher::Observer::OnSystemInfoChanged(
    bool show,
    const std::string& os_version_label_text,
    const std::string& enterprise_info_text,
    const std::string& bluetooth_name) {}

void LoginDataDispatcher::Observer::OnPublicSessionDisplayNameChanged(
    const AccountId& account_id,
    const std::string& display_name) {}

void LoginDataDispatcher::Observer::OnPublicSessionLocalesChanged(
    const AccountId& account_id,
    const std::vector<mojom::LocaleItemPtr>& locales,
    const std::string& default_locale,
    bool show_advanced_view) {}

void LoginDataDispatcher::Observer::OnPublicSessionKeyboardLayoutsChanged(
    const AccountId& account_id,
    const std::string& locale,
    const std::vector<mojom::InputMethodItemPtr>& keyboard_layouts) {}

void LoginDataDispatcher::Observer::OnDetachableBasePairingStatusChanged(
    DetachableBasePairingStatus pairing_status) {}

LoginDataDispatcher::LoginDataDispatcher() = default;

LoginDataDispatcher::~LoginDataDispatcher() = default;

void LoginDataDispatcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LoginDataDispatcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void LoginDataDispatcher::NotifyUsers(
    const std::vector<mojom::LoginUserInfoPtr>& users) {
  for (auto& observer : observers_)
    observer.OnUsersChanged(users);
}

void LoginDataDispatcher::SetPinEnabledForUser(const AccountId& user,
                                               bool enabled) {
  for (auto& observer : observers_)
    observer.OnPinEnabledForUserChanged(user, enabled);
}

void LoginDataDispatcher::SetFingerprintState(const AccountId& account_id,
                                              mojom::FingerprintState state) {
  for (auto& observer : observers_)
    observer.OnFingerprintStateChanged(account_id, state);
}

void LoginDataDispatcher::NotifyFingerprintAuthResult(
    const AccountId& account_id,
    bool successful) {
  for (auto& observer : observers_)
    observer.OnFingerprintAuthResult(account_id, successful);
}

void LoginDataDispatcher::SetAuthEnabledForUser(
    const AccountId& account_id,
    bool is_enabled,
    base::Optional<base::Time> auth_reenabled_time) {
  for (auto& observer : observers_) {
    observer.OnAuthEnabledForUserChanged(account_id, is_enabled,
                                         auth_reenabled_time);
  }
}

void LoginDataDispatcher::SetTapToUnlockEnabledForUser(const AccountId& user,
                                                       bool enabled) {
  for (auto& observer : observers_)
    observer.OnTapToUnlockEnabledForUserChanged(user, enabled);
}

void LoginDataDispatcher::SetForceOnlineSignInForUser(const AccountId& user) {
  for (auto& observer : observers_)
    observer.OnForceOnlineSignInForUser(user);
}

void LoginDataDispatcher::SetLockScreenNoteState(mojom::TrayActionState state) {
  for (auto& observer : observers_)
    observer.OnLockScreenNoteStateChanged(state);
}

void LoginDataDispatcher::ShowEasyUnlockIcon(
    const AccountId& user,
    const mojom::EasyUnlockIconOptionsPtr& icon) {
  for (auto& observer : observers_)
    observer.OnShowEasyUnlockIcon(user, icon);
}

void LoginDataDispatcher::ShowWarningBanner(const base::string16& message) {
  for (auto& observer : observers_)
    observer.OnShowWarningBanner(message);
}

void LoginDataDispatcher::HideWarningBanner() {
  for (auto& observer : observers_)
    observer.OnHideWarningBanner();
}

void LoginDataDispatcher::SetSystemInfo(
    bool show_if_hidden,
    const std::string& os_version_label_text,
    const std::string& enterprise_info_text,
    const std::string& bluetooth_name) {
  for (auto& observer : observers_) {
    observer.OnSystemInfoChanged(show_if_hidden, os_version_label_text,
                                 enterprise_info_text, bluetooth_name);
  }
}

void LoginDataDispatcher::SetPublicSessionDisplayName(
    const AccountId& account_id,
    const std::string& display_name) {
  for (auto& observer : observers_)
    observer.OnPublicSessionDisplayNameChanged(account_id, display_name);
}

void LoginDataDispatcher::SetPublicSessionLocales(
    const AccountId& account_id,
    const std::vector<mojom::LocaleItemPtr>& locales,
    const std::string& default_locale,
    bool show_advanced_view) {
  for (auto& observer : observers_) {
    observer.OnPublicSessionLocalesChanged(account_id, locales, default_locale,
                                           show_advanced_view);
  }
}

void LoginDataDispatcher::SetPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale,
    const std::vector<mojom::InputMethodItemPtr>& keyboard_layouts) {
  for (auto& observer : observers_) {
    observer.OnPublicSessionKeyboardLayoutsChanged(account_id, locale,
                                                   keyboard_layouts);
  }
}

void LoginDataDispatcher::SetDetachableBasePairingStatus(
    DetachableBasePairingStatus pairing_status) {
  for (auto& observer : observers_)
    observer.OnDetachableBasePairingStatusChanged(pairing_status);
}

}  // namespace ash
