// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_DATA_DISPATCHER_H_
#define ASH_LOGIN_UI_LOGIN_DATA_DISPATCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/detachable_base/detachable_base_pairing_status.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/mojom/tray_action.mojom.h"
#include "base/observer_list.h"
#include "base/time/time.h"

class AccountId;

namespace ash {

// Provides access to data needed by the lock/login screen. The login UI will
// register observers which are then invoked when data is posted to the data
// dispatcher.
//
// This provides access to data notification events only.
// LoginDataDispatcher is not responsible for owning data (the login
// embedder should own the data). This type provides a clean interface between
// the actual view/UI implemenation and the embedder.
//
// There are various types which provide data to LoginDataDispatcher. For
// example, the lock screen uses the session manager, whereas the login screen
// uses the user manager. The debug overlay proxies the original data dispatcher
// so it can provide fake state from an arbitrary source.
class ASH_EXPORT LoginDataDispatcher : public LoginScreenModel {
 public:
  // Types interested in login state should derive from |Observer| and register
  // themselves on the |LoginDataDispatcher| instance passed to the view
  // hierarchy.
  class Observer {
   public:
    virtual ~Observer();

    // Called when the displayed set of users has changed.
    virtual void OnUsersChanged(const std::vector<LoginUserInfo>& users);

    // Called when |avatar| for |account_id| has changed.
    virtual void OnUserAvatarChanged(const AccountId& account_id,
                                     const UserAvatar& avatar);

    // Called when auth factors availability changed for |user|. By
    // default, password should be enabled, pin and challenge-response
    // authentication should be disabled.
    virtual void OnUserAuthFactorsChanged(
        const AccountId& user,
        cryptohome::AuthFactorsSet auth_factors,
        cryptohome::PinLockAvailability pin_available_at);

    // Called when pin should be enabled or disabled for |user|. By default, pin
    // should be disabled.
    virtual void OnPinEnabledForUserChanged(
        const AccountId& user,
        bool enabled,
        cryptohome::PinLockAvailability available_at);

    // Called when the challenge-response authentication should be enabled or
    // disabled for |user|. By default, it should be disabled.
    virtual void OnChallengeResponseAuthEnabledForUserChanged(
        const AccountId& user,
        bool enabled);

    // Called when fingerprint unlock state changes for user with |account_id|.
    virtual void OnFingerprintStateChanged(const AccountId& account_id,
                                           FingerprintState state);

    // Called after a fingerprint authentication attempt.
    virtual void OnFingerprintAuthResult(const AccountId& account_id,
                                         bool successful);

    // Called after unlock was aborted after successful auth attempt.
    virtual void OnResetFingerprintUIState(const AccountId& account_id);

    // Called when smart lock state is changed.
    virtual void OnSmartLockStateChanged(const AccountId& user,
                                         SmartLockState state);

    // Called after a smart lock authentication attempt.
    virtual void OnSmartLockAuthResult(const AccountId& account_id,
                                       bool successful);

    // Called when auth should be enabled for |user|. By default, auth should be
    // enabled.
    virtual void OnAuthEnabledForUser(const AccountId& user);

    // Called when auth should be disabled for |user|. By default, auth should
    // be enabled.
    virtual void OnAuthDisabledForUser(
        const AccountId& user,
        const AuthDisabledData& auth_disabled_data);

    // Called when authentication stage changed.
    virtual void OnAuthenticationStageChanged(AuthenticationStage auth_stage);

    // Called when TPM is locked.
    virtual void OnSetTpmLockedState(const AccountId& user,
                                     bool is_locked,
                                     base::TimeDelta time_left);

    // Called when the given user can click their pod to unlock.
    virtual void OnTapToUnlockEnabledForUserChanged(const AccountId& user,
                                                    bool enabled);

    // Called when |user| must authenticate online (e.g. when OAuth refresh
    // token is revoked).
    virtual void OnForceOnlineSignInForUser(const AccountId& user);

    // Called when the lock screen note state changes.
    virtual void OnLockScreenNoteStateChanged(mojom::TrayActionState state);

    // Called when a warning message should be displayed, or hidden if |message|
    // is empty.
    virtual void OnWarningMessageUpdated(const std::u16string& message);

    // Called when the system info has changed.
    virtual void OnSystemInfoChanged(bool show,
                                     bool enforced,
                                     const std::string& os_version_label_text,
                                     const std::string& enterprise_info_text,
                                     const std::string& bluetooth_name,
                                     bool adb_sideloading_enabled);

    // Called when public session display name is changed for user with
    // |account_id|.
    virtual void OnPublicSessionDisplayNameChanged(
        const AccountId& account_id,
        const std::string& display_name);

    // Called when public session locales are changed for user with
    // |account_id|.
    virtual void OnPublicSessionLocalesChanged(
        const AccountId& account_id,
        const std::vector<LocaleItem>& locales,
        const std::string& default_locale,
        bool show_advanced_view);

    // Called when public session keyboard layouts are changed for user with
    // |account_id|.
    virtual void OnPublicSessionKeyboardLayoutsChanged(
        const AccountId& account_id,
        const std::string& locale,
        const std::vector<InputMethodItem>& keyboard_layouts);

    // Called when conditions for showing full management disclosure message
    // are changed.
    virtual void OnPublicSessionShowFullManagementDisclosureChanged(
        bool show_full_management_disclosure);

    // Called when the pairing status of detachable base changes - e.g. when the
    // base is attached or detached.
    virtual void OnDetachableBasePairingStatusChanged(
        DetachableBasePairingStatus pairing_status);

    // Called when focus is leaving a lock screen app window due to tabbing.
    // |reverse| - whether the tab order is reversed.
    virtual void OnFocusLeavingLockScreenApps(bool reverse);

    // Called when the state of the OOBE dialog is changed.
    virtual void OnOobeDialogStateChanged(OobeDialogState state);

    // Called when the focused pod is changed on the login screen with the
    // corresponding `account_id`. In case all the pods lost focus the
    // `EmptyAccountId` passed as the argument.
    virtual void OnFocusPod(const AccountId& account_id);
  };

  LoginDataDispatcher();

  LoginDataDispatcher(const LoginDataDispatcher&) = delete;
  LoginDataDispatcher& operator=(const LoginDataDispatcher&) = delete;

  ~LoginDataDispatcher() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // LoginScreenModel:
  // TODO(estade): for now, LoginScreenModel overrides are mixed with
  // non-virtual methods. More of the non-virtual methods will become a part of
  // the LoginScreenModel interface, so ordering is being preserved. When
  // LoginScreenModel is complete, separate out the methods that aren't
  // overrides.
  void SetUserList(const std::vector<LoginUserInfo>& users) override;
  void SetAuthFactorsForUser(
      const AccountId& user,
      cryptohome::AuthFactorsSet auth_factors,
      cryptohome::PinLockAvailability pin_available_at) override;
  void SetPinEnabledForUser(
      const AccountId& user,
      bool enabled,
      cryptohome::PinLockAvailability available_at) override;
  void SetChallengeResponseAuthEnabledForUser(const AccountId& user,
                                              bool enabled) override;
  void SetAvatarForUser(const AccountId& account_id,
                        const UserAvatar& avatar) override;
  void SetFingerprintState(const AccountId& account_id,
                           FingerprintState state) override;
  void NotifyFingerprintAuthResult(const AccountId& account_id,
                                   bool successful) override;
  void ResetFingerprintUIState(const AccountId& account_id) override;
  void SetSmartLockState(const AccountId& user, SmartLockState state) override;
  void NotifySmartLockAuthResult(const AccountId& account_id,
                                 bool successful) override;
  void EnableAuthForUser(const AccountId& account_id) override;
  void DisableAuthForUser(const AccountId& account_id,
                          const AuthDisabledData& auth_disabled_data) override;
  void AuthenticationStageChange(const AuthenticationStage auth_stage) override;
  void SetTpmLockedState(const AccountId& user,
                         bool is_locked,
                         base::TimeDelta time_left) override;
  void SetTapToUnlockEnabledForUser(const AccountId& user,
                                    bool enabled) override;
  void ForceOnlineSignInForUser(const AccountId& user) override;
  void SetLockScreenNoteState(mojom::TrayActionState state);
  void UpdateWarningMessage(const std::u16string& message) override;
  void SetSystemInfo(bool show,
                     bool enforced,
                     const std::string& os_version_label_text,
                     const std::string& enterprise_info_text,
                     const std::string& bluetooth_name,
                     bool adb_sideloading_enabled) override;
  void SetPublicSessionDisplayName(const AccountId& account_id,
                                   const std::string& display_name) override;
  void SetPublicSessionLocales(const AccountId& account_id,
                               const std::vector<LocaleItem>& locales,
                               const std::string& default_locale,
                               bool show_advanced_view) override;
  void SetPublicSessionKeyboardLayouts(
      const AccountId& account_id,
      const std::string& locale,
      const std::vector<InputMethodItem>& keyboard_layouts) override;
  void SetPublicSessionShowFullManagementDisclosure(
      bool show_full_management_disclosure) override;
  void SetDetachableBasePairingStatus(
      DetachableBasePairingStatus pairing_status);
  void HandleFocusLeavingLockScreenApps(bool reverse) override;
  void NotifyOobeDialogState(OobeDialogState state) override;
  void NotifyFocusPod(const AccountId& account_id) override;

 private:
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_DATA_DISPATCHER_H_
