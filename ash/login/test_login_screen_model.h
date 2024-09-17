// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_TEST_LOGIN_SCREEN_MODEL_H_
#define ASH_LOGIN_TEST_LOGIN_SCREEN_MODEL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/login_screen_model.h"

class AccountId;

class ASH_EXPORT TestLoginScreenModel : public ash::LoginScreenModel {
 public:
  TestLoginScreenModel();

  TestLoginScreenModel(const TestLoginScreenModel&) = delete;
  TestLoginScreenModel& operator=(const TestLoginScreenModel&) = delete;

  ~TestLoginScreenModel() override;

  // ash::LoginScreenModel:
  void SetUserList(const std::vector<ash::LoginUserInfo>& users) override;
  void SetAuthFactorsForUser(
      const AccountId& user,
      cryptohome::AuthFactorsSet auth_factors,
      cryptohome::PinLockAvailability pin_available_at) override;
  void SetPinEnabledForUser(
      const AccountId& account_id,
      bool is_enabled,
      cryptohome::PinLockAvailability available_at) override;
  void SetAvatarForUser(const AccountId& account_id,
                        const ash::UserAvatar& avatar) override;
  void SetFingerprintState(const AccountId& account_id,
                           ash::FingerprintState state) override;
  void NotifyFingerprintAuthResult(const AccountId& account_id,
                                   bool successful) override;
  void ResetFingerprintUIState(const AccountId& account_id) override;
  void SetSmartLockState(const AccountId& account_id,
                         ash::SmartLockState state) override;
  void NotifySmartLockAuthResult(const AccountId& account_id,
                                 bool successful) override;
  void EnableAuthForUser(const AccountId& account_id) override;
  void DisableAuthForUser(
      const AccountId& account_id,
      const ash::AuthDisabledData& auth_disabled_data) override;
  void AuthenticationStageChange(ash::AuthenticationStage auth_state) override;

  void SetTpmLockedState(const AccountId& user,
                         bool is_locked,
                         base::TimeDelta time_left) override;
  void SetTapToUnlockEnabledForUser(const AccountId& account_id,
                                    bool enabled) override;
  void ForceOnlineSignInForUser(const AccountId& account_id) override;
  void SetChallengeResponseAuthEnabledForUser(const AccountId& user,
                                              bool enabled) override;

  void UpdateWarningMessage(const std::u16string& message) override;
  void SetSystemInfo(bool show,
                     bool enforced,
                     const std::string& os_version_label_text,
                     const std::string& enterprise_info_text,
                     const std::string& bluetooth_name,
                     bool adb_sideloading_enabled) override;
  void SetPublicSessionLocales(const AccountId& account_id,
                               const std::vector<ash::LocaleItem>& locales,
                               const std::string& default_locale,
                               bool show_advanced_view) override;
  void SetPublicSessionDisplayName(const AccountId& account_id,
                                   const std::string& display_name) override;
  void SetPublicSessionKeyboardLayouts(
      const AccountId& account_id,
      const std::string& locale,
      const std::vector<ash::InputMethodItem>& keyboard_layouts) override;
  void SetPublicSessionShowFullManagementDisclosure(
      bool show_full_management_disclosure) override;
  void HandleFocusLeavingLockScreenApps(bool reverse) override;
  void NotifyOobeDialogState(ash::OobeDialogState state) override;
  void NotifyFocusPod(const AccountId& account_id) override;
};

#endif  // ASH_LOGIN_TEST_LOGIN_SCREEN_MODEL_H_
