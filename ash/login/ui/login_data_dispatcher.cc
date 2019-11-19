// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_data_dispatcher.h"

namespace ash {

LoginDataDispatcher::Observer::~Observer() = default;

void LoginDataDispatcher::Observer::OnUsersChanged(
    const std::vector<LoginUserInfo>& users) {}

void LoginDataDispatcher::Observer::OnUserAvatarChanged(
    const AccountId& account_id,
    const UserAvatar& avatar) {}

void LoginDataDispatcher::Observer::OnPinEnabledForUserChanged(
    const AccountId& user,
    bool enabled) {}

void LoginDataDispatcher::Observer::
    OnChallengeResponseAuthEnabledForUserChanged(const AccountId& user,
                                                 bool enabled) {}

void LoginDataDispatcher::Observer::OnFingerprintStateChanged(
    const AccountId& account_id,
    FingerprintState state) {}

void LoginDataDispatcher::Observer::OnFingerprintAuthResult(
    const AccountId& account_id,
    bool successful) {}

void LoginDataDispatcher::Observer::OnAuthEnabledForUser(
    const AccountId& user) {}

void LoginDataDispatcher::Observer::OnAuthDisabledForUser(
    const AccountId& user,
    const AuthDisabledData& auth_disabled_data) {}

void LoginDataDispatcher::Observer::OnTapToUnlockEnabledForUserChanged(
    const AccountId& user,
    bool enabled) {}

void LoginDataDispatcher::Observer::OnForceOnlineSignInForUser(
    const AccountId& user) {}

void LoginDataDispatcher::Observer::OnLockScreenNoteStateChanged(
    mojom::TrayActionState state) {}

void LoginDataDispatcher::Observer::OnShowEasyUnlockIcon(
    const AccountId& user,
    const EasyUnlockIconOptions& icon) {}

void LoginDataDispatcher::Observer::OnWarningMessageUpdated(
    const base::string16& message) {}

void LoginDataDispatcher::Observer::OnSystemInfoChanged(
    bool show,
    bool enforced,
    const std::string& os_version_label_text,
    const std::string& enterprise_info_text,
    const std::string& bluetooth_name,
    bool adb_sideloading_enabled) {}

void LoginDataDispatcher::Observer::OnPublicSessionDisplayNameChanged(
    const AccountId& account_id,
    const std::string& display_name) {}

void LoginDataDispatcher::Observer::OnPublicSessionLocalesChanged(
    const AccountId& account_id,
    const std::vector<LocaleItem>& locales,
    const std::string& default_locale,
    bool show_advanced_view) {}

void LoginDataDispatcher::Observer::OnPublicSessionKeyboardLayoutsChanged(
    const AccountId& account_id,
    const std::string& locale,
    const std::vector<InputMethodItem>& keyboard_layouts) {}

void LoginDataDispatcher::Observer::
    OnPublicSessionShowFullManagementDisclosureChanged(
        bool show_full_management_disclosure) {}

void LoginDataDispatcher::Observer::OnDetachableBasePairingStatusChanged(
    DetachableBasePairingStatus pairing_status) {}

void LoginDataDispatcher::Observer::OnFocusLeavingLockScreenApps(bool reverse) {
}

void LoginDataDispatcher::Observer::OnOobeDialogStateChanged(
    OobeDialogState state) {}

LoginDataDispatcher::LoginDataDispatcher() = default;

LoginDataDispatcher::~LoginDataDispatcher() = default;

void LoginDataDispatcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LoginDataDispatcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void LoginDataDispatcher::SetUserList(const std::vector<LoginUserInfo>& users) {
  for (auto& observer : observers_)
    observer.OnUsersChanged(users);
}

void LoginDataDispatcher::SetPinEnabledForUser(const AccountId& user,
                                               bool enabled) {
  // Chrome will update pin pod state every time user tries to authenticate.
  // LockScreen is destroyed in the case of authentication success.
  for (auto& observer : observers_)
    observer.OnPinEnabledForUserChanged(user, enabled);
}

void LoginDataDispatcher::SetChallengeResponseAuthEnabledForUser(
    const AccountId& user,
    bool enabled) {
  for (auto& observer : observers_)
    observer.OnChallengeResponseAuthEnabledForUserChanged(user, enabled);
}

void LoginDataDispatcher::SetFingerprintState(const AccountId& account_id,
                                              FingerprintState state) {
  for (auto& observer : observers_)
    observer.OnFingerprintStateChanged(account_id, state);
}

void LoginDataDispatcher::SetAvatarForUser(const AccountId& account_id,
                                           const UserAvatar& avatar) {
  for (auto& observer : observers_)
    observer.OnUserAvatarChanged(account_id, avatar);
}

void LoginDataDispatcher::NotifyFingerprintAuthResult(
    const AccountId& account_id,
    bool successful) {
  for (auto& observer : observers_)
    observer.OnFingerprintAuthResult(account_id, successful);
}

void LoginDataDispatcher::EnableAuthForUser(const AccountId& account_id) {
  for (auto& observer : observers_)
    observer.OnAuthEnabledForUser(account_id);
}

void LoginDataDispatcher::DisableAuthForUser(
    const AccountId& account_id,
    const AuthDisabledData& auth_disabled_data) {
  for (auto& observer : observers_)
    observer.OnAuthDisabledForUser(account_id, auth_disabled_data);
}

void LoginDataDispatcher::SetTapToUnlockEnabledForUser(const AccountId& user,
                                                       bool enabled) {
  for (auto& observer : observers_)
    observer.OnTapToUnlockEnabledForUserChanged(user, enabled);
}

void LoginDataDispatcher::ForceOnlineSignInForUser(const AccountId& user) {
  for (auto& observer : observers_)
    observer.OnForceOnlineSignInForUser(user);
}

void LoginDataDispatcher::SetLockScreenNoteState(mojom::TrayActionState state) {
  for (auto& observer : observers_)
    observer.OnLockScreenNoteStateChanged(state);
}

void LoginDataDispatcher::ShowEasyUnlockIcon(
    const AccountId& user,
    const EasyUnlockIconOptions& icon) {
  for (auto& observer : observers_)
    observer.OnShowEasyUnlockIcon(user, icon);
}

void LoginDataDispatcher::UpdateWarningMessage(const base::string16& message) {
  for (auto& observer : observers_)
    observer.OnWarningMessageUpdated(message);
}

void LoginDataDispatcher::SetSystemInfo(
    bool show,
    bool enforced,
    const std::string& os_version_label_text,
    const std::string& enterprise_info_text,
    const std::string& bluetooth_name,
    bool adb_sideloading_enabled) {
  for (auto& observer : observers_) {
    observer.OnSystemInfoChanged(show, enforced, os_version_label_text,
                                 enterprise_info_text, bluetooth_name,
                                 adb_sideloading_enabled);
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
    const std::vector<LocaleItem>& locales,
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
    const std::vector<InputMethodItem>& keyboard_layouts) {
  for (auto& observer : observers_) {
    observer.OnPublicSessionKeyboardLayoutsChanged(account_id, locale,
                                                   keyboard_layouts);
  }
}

void LoginDataDispatcher::SetPublicSessionShowFullManagementDisclosure(
    bool show_full_management_disclosure) {
  for (auto& observer : observers_) {
    observer.OnPublicSessionShowFullManagementDisclosureChanged(
        show_full_management_disclosure);
  }
}

void LoginDataDispatcher::SetDetachableBasePairingStatus(
    DetachableBasePairingStatus pairing_status) {
  for (auto& observer : observers_)
    observer.OnDetachableBasePairingStatusChanged(pairing_status);
}

void LoginDataDispatcher::HandleFocusLeavingLockScreenApps(bool reverse) {
  for (auto& observer : observers_)
    observer.OnFocusLeavingLockScreenApps(reverse);
}

void LoginDataDispatcher::NotifyOobeDialogState(OobeDialogState state) {
  for (auto& observer : observers_)
    observer.OnOobeDialogStateChanged(state);
}

}  // namespace ash
