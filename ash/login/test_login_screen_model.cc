// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/test_login_screen_model.h"

class AccountId;

TestLoginScreenModel::TestLoginScreenModel() = default;
TestLoginScreenModel::~TestLoginScreenModel() = default;

void TestLoginScreenModel::SetUserList(
    const std::vector<ash::LoginUserInfo>& users) {}
void TestLoginScreenModel::SetAuthFactorsForUser(
    const AccountId& user,
    cryptohome::AuthFactorsSet auth_factors,
    cryptohome::PinLockAvailability pin_available_at) {}
void TestLoginScreenModel::SetPinEnabledForUser(
    const AccountId& account_id,
    bool is_enabled,
    cryptohome::PinLockAvailability available_at) {}
void TestLoginScreenModel::SetAvatarForUser(const AccountId& account_id,
                                            const ash::UserAvatar& avatar) {}
void TestLoginScreenModel::SetFingerprintState(const AccountId& account_id,
                                               ash::FingerprintState state) {}
void TestLoginScreenModel::NotifyFingerprintAuthResult(
    const AccountId& account_id,
    bool successful) {}
void TestLoginScreenModel::ResetFingerprintUIState(
    const AccountId& account_id) {}
void TestLoginScreenModel::SetSmartLockState(const AccountId& account_id,
                                             ash::SmartLockState state) {}
void TestLoginScreenModel::NotifySmartLockAuthResult(
    const AccountId& account_id,
    bool successful) {}
void TestLoginScreenModel::EnableAuthForUser(const AccountId& account_id) {}
void TestLoginScreenModel::DisableAuthForUser(
    const AccountId& account_id,
    const ash::AuthDisabledData& auth_disabled_data) {}
void TestLoginScreenModel::AuthenticationStageChange(
    ash::AuthenticationStage auth_state) {}

void TestLoginScreenModel::SetTpmLockedState(const AccountId& user,
                                             bool is_locked,
                                             base::TimeDelta time_left) {}
void TestLoginScreenModel::SetTapToUnlockEnabledForUser(
    const AccountId& account_id,
    bool enabled) {}
void TestLoginScreenModel::ForceOnlineSignInForUser(
    const AccountId& account_id) {}
void TestLoginScreenModel::SetChallengeResponseAuthEnabledForUser(
    const AccountId& user,
    bool enabled) {}
void TestLoginScreenModel::UpdateWarningMessage(const std::u16string& message) {
}
void TestLoginScreenModel::SetSystemInfo(
    bool show,
    bool enforced,
    const std::string& os_version_label_text,
    const std::string& enterprise_info_text,
    const std::string& bluetooth_name,
    bool adb_sideloading_enabled) {}
void TestLoginScreenModel::SetPublicSessionDisplayName(
    const AccountId& account_id,
    const std::string& display_name) {}
void TestLoginScreenModel::SetPublicSessionLocales(
    const AccountId& account_id,
    const std::vector<ash::LocaleItem>& locales,
    const std::string& default_locale,
    bool show_advanced_view) {}
void TestLoginScreenModel::SetPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale,
    const std::vector<ash::InputMethodItem>& keyboard_layouts) {}
void TestLoginScreenModel::SetPublicSessionShowFullManagementDisclosure(
    bool show_full_management_disclosure) {}
void TestLoginScreenModel::HandleFocusLeavingLockScreenApps(bool reverse) {}
void TestLoginScreenModel::NotifyOobeDialogState(ash::OobeDialogState state) {}
void TestLoginScreenModel::NotifyFocusPod(const AccountId& account_id) {}
