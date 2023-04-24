// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_SMARTLOCK_STATE_HANDLER_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_SMARTLOCK_STATE_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "components/account_id/account_id.h"

namespace ash {

enum class SmartLockState;

// Profile specific class responsible for updating screenlock UI for the user
// associated with the profile when their Easy Unlock state changes.
class SmartLockStateHandler
    : public proximity_auth::ScreenlockBridge::Observer {
 public:
  // `account_id`: The account id of the user associated with the profile to
  //     which this class is attached.
  // `screenlock_bridge`: The screenlock bridge used to update the Smart Lock
  //     state.
  SmartLockStateHandler(const AccountId& account_id,
                        proximity_auth::ScreenlockBridge* screenlock_bridge);

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
  raw_ptr<proximity_auth::ScreenlockBridge, ExperimentalAsh>
      screenlock_bridge_ = nullptr;

  // Whether the user's phone was ever locked while on the current lock screen.
  bool did_see_locked_phone_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_SMARTLOCK_STATE_HANDLER_H_
