// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_SMARTLOCK_STATE_HANDLER_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_SMARTLOCK_STATE_HANDLER_H_

#include <string>

#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "components/account_id/account_id.h"

namespace proximity_auth {
class ProximityAuthPrefManager;
}  // namespace proximity_auth

namespace ash {

enum class SmartLockState;

// Profile specific class responsible for updating screenlock UI for the user
// associated with the profile when their Easy Unlock state changes.
class SmartLockStateHandler
    : public proximity_auth::ScreenlockBridge::Observer {
 public:
  // Hard lock states.
  enum HardlockState {
    NO_HARDLOCK = 0,           // Hard lock is not enforced. This is default.
    USER_HARDLOCK = 1 << 0,    // Hard lock is requested by user.
    PAIRING_CHANGED = 1 << 1,  // Hard lock because pairing data is changed.
    NO_PAIRING = 1 << 2,       // Hard lock because there is no pairing data.
    LOGIN_FAILED = 1 << 3,     // Transient hard lock caused by login attempt
                               // failure. Reset when screen is unlocked.
    PAIRING_ADDED = 1 << 4,    // Similar to PAIRING_CHANGED when it happens
                               // on a new Chromebook.
    LOGIN_DISABLED = 1 << 5,   // Sign-in via Smart Lock is disabled in
                               // Settings.
  };

  // `account_id`: The account id of the user associated with the profile to
  //     which this class is attached.
  // `initial_hardlock_state`: The initial hardlock state.
  // `screenlock_bridge`: The screenlock bridge used to update the Smart Lock
  //     state.
  // `pref_manager`: Used primarily to track if the "Signin with Smart Lock is
  //     disabled" message has been shown before.
  SmartLockStateHandler(const AccountId& account_id,
                        HardlockState initial_hardlock_state,
                        proximity_auth::ScreenlockBridge* screenlock_bridge,
                        proximity_auth::ProximityAuthPrefManager* pref_manager);

  SmartLockStateHandler(const SmartLockStateHandler&) = delete;
  SmartLockStateHandler& operator=(const SmartLockStateHandler&) = delete;

  ~SmartLockStateHandler() override;

  // Returns true if handler is not in INACTIVE state.
  bool IsActive() const;

  // Whether the handler is in state that is allowed just after auth failure
  // (i.e. the state that would cause auth failure rather than one caused by an
  // auth failure).
  bool InStateValidOnRemoteAuthFailure() const;

  // Changes internal state to `new_state` and updates the user's Smart Lock
  // state accordingly.
  void ChangeState(SmartLockState new_state);

  // Updates the hardlock state.
  void SetHardlockState(HardlockState new_state);

  // Shows the hardlock UI if the hardlock_state_ is not NO_HARDLOCK.
  void MaybeShowHardlockUI();

  SmartLockState state() const { return state_; }

 private:
  // proximity_auth::ScreenlockBridge::Observer:
  void OnScreenDidLock(proximity_auth::ScreenlockBridge::LockHandler::ScreenType
                           screen_type) override;
  void OnScreenDidUnlock(
      proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type)
      override;
  void OnFocusedUserChanged(const AccountId& account_id) override;

  // Forces refresh of the Smart Lock UI.
  void RefreshSmartLockState();

  void ShowHardlockUI();

  // Updates icon's tooltip options.
  void UpdateTooltipOptions(
      proximity_auth::ScreenlockBridge::UserPodCustomIconInfo* icon_info);

  // Gets the name to be used for the device. The name depends on the device
  // type (example values: Chromebook and Chromebox).
  std::u16string GetDeviceName();

  // Updates the screenlock auth type if it has to be changed.
  void UpdateScreenlockAuthType();

  SmartLockState state_;
  const AccountId account_id_;
  proximity_auth::ScreenlockBridge* screenlock_bridge_ = nullptr;
  proximity_auth::ProximityAuthPrefManager* pref_manager_ = nullptr;

  // State of hardlock.
  HardlockState hardlock_state_;
  bool hardlock_ui_shown_ = false;

  // Whether the user's phone was ever locked while on the current lock screen.
  bool did_see_locked_phone_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_SMARTLOCK_STATE_HANDLER_H_
