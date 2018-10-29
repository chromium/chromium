// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_DATA_DISPATCHER_H_
#define ASH_LOGIN_UI_LOGIN_DATA_DISPATCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/detachable_base/detachable_base_pairing_status.h"
#include "ash/public/interfaces/login_user_info.mojom.h"
#include "ash/public/interfaces/tray_action.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

// Provides access to data needed by the lock/login screen. The login UI will
// register observers which are then invoked when data is posted to the data
// dispatcher.
//
// This provides access to data notification events only. LoginDataDispatcher is
// not responsible for owning data (the login embedder should own the data).
// This type provides a clean interface between the actual view/UI implemenation
// and the embedder.
//
// There are various types which provide data to LoginDataDispatcher. For
// example, the lock screen uses the session manager, whereas the login screen
// uses the user manager. The debug overlay proxies the original data dispatcher
// so it can provide fake state from an arbitrary source.
class ASH_EXPORT LoginDataDispatcher {
 public:
  // Types interested in login state should derive from |Observer| and register
  // themselves on the |LoginDataDispatcher| instance passed to the view
  // hierarchy.
  class Observer {
   public:
    virtual ~Observer();

    // Called when the displayed set of users has changed.
    virtual void OnUsersChanged(
        const std::vector<mojom::LoginUserInfoPtr>& users);

    // Called when pin should be enabled or disabled for |user|. By default, pin
    // should be disabled.
    virtual void OnPinEnabledForUserChanged(const AccountId& user,
                                            bool enabled);

    // Called when fingerprint unlock state changes for user with |account_id|.
    virtual void OnFingerprintStateChanged(const AccountId& account_id,
                                           mojom::FingerprintState state);

    // Called after a fingerprint authentication attempt.
    virtual void OnFingerprintAuthResult(const AccountId& account_id,
                                         bool successful);

    // Called when auth should be enabled or disabled for |user|. By default,
    // auth should be enabled.
    virtual void OnAuthEnabledForUserChanged(
        const AccountId& user,
        bool enabled,
        const base::Optional<base::Time>& auth_reenabled_time);

    // Called when the given user can click their pod to unlock.
    virtual void OnTapToUnlockEnabledForUserChanged(const AccountId& user,
                                                    bool enabled);

    // Called when |user| must authenticate online (e.g. when OAuth refresh
    // token is revoked).
    virtual void OnForceOnlineSignInForUser(const AccountId& user);

    // Called when the lock screen note state changes.
    virtual void OnLockScreenNoteStateChanged(mojom::TrayActionState state);

    // Called when an easy unlock icon should be displayed.
    virtual void OnShowEasyUnlockIcon(
        const AccountId& user,
        const mojom::EasyUnlockIconOptionsPtr& icon);

    // Called when a warning banner message should be displayed.
    virtual void OnShowWarningBanner(const base::string16& message);

    // Called when a warning banner message should be hidden.
    virtual void OnHideWarningBanner();

    // Called when the system info has changed.
    virtual void OnSystemInfoChanged(bool show,
                                     const std::string& os_version_label_text,
                                     const std::string& enterprise_info_text,
                                     const std::string& bluetooth_name);

    // Called when public session display name is changed for user with
    // |account_id|.
    virtual void OnPublicSessionDisplayNameChanged(
        const AccountId& account_id,
        const std::string& display_name);

    // Called when public session locales are changed for user with
    // |account_id|.
    virtual void OnPublicSessionLocalesChanged(
        const AccountId& account_id,
        const std::vector<mojom::LocaleItemPtr>& locales,
        const std::string& default_locale,
        bool show_advanced_view);

    // Called when public session keyboard layouts are changed for user with
    // |account_id|.
    virtual void OnPublicSessionKeyboardLayoutsChanged(
        const AccountId& account_id,
        const std::string& locale,
        const std::vector<mojom::InputMethodItemPtr>& keyboard_layouts);

    // Called when the pairing status of detachable base changes - e.g. when the
    // base is attached or detached.
    virtual void OnDetachableBasePairingStatusChanged(
        DetachableBasePairingStatus pairing_status);
  };

  LoginDataDispatcher();
  ~LoginDataDispatcher();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyUsers(const std::vector<mojom::LoginUserInfoPtr>& users);
  void SetPinEnabledForUser(const AccountId& user, bool enabled);
  void SetFingerprintState(const AccountId& account_id,
                           mojom::FingerprintState state);
  void NotifyFingerprintAuthResult(const AccountId& account_id,
                                   bool successful);
  void SetAuthEnabledForUser(const AccountId& account_id,
                             bool is_enabled,
                             base::Optional<base::Time> auth_reenabled_time);
  void SetTapToUnlockEnabledForUser(const AccountId& user, bool enabled);
  void SetForceOnlineSignInForUser(const AccountId& user);
  void SetLockScreenNoteState(mojom::TrayActionState state);
  void ShowEasyUnlockIcon(const AccountId& user,
                          const mojom::EasyUnlockIconOptionsPtr& icon);
  void ShowWarningBanner(const base::string16& message);
  void HideWarningBanner();
  void SetSystemInfo(bool show_if_hidden,
                     const std::string& os_version_label_text,
                     const std::string& enterprise_info_text,
                     const std::string& bluetooth_name);
  void SetPublicSessionDisplayName(const AccountId& account_id,
                                   const std::string& display_name);
  void SetPublicSessionLocales(const AccountId& account_id,
                               const std::vector<mojom::LocaleItemPtr>& locales,
                               const std::string& default_locale,
                               bool show_advanced_view);
  void SetPublicSessionKeyboardLayouts(
      const AccountId& account_id,
      const std::string& locale,
      const std::vector<mojom::InputMethodItemPtr>& keyboard_layouts);
  void SetDetachableBasePairingStatus(
      DetachableBasePairingStatus pairing_status);

 private:
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(LoginDataDispatcher);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_DATA_DISPATCHER_H_
