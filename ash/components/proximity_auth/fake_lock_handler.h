// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PROXIMITY_AUTH_FAKE_LOCK_HANDLER_H_
#define ASH_COMPONENTS_PROXIMITY_AUTH_FAKE_LOCK_HANDLER_H_

#include "ash/components/proximity_auth/screenlock_bridge.h"

namespace proximity_auth {

class FakeLockHandler : public ScreenlockBridge::LockHandler {
 public:
  FakeLockHandler();

  FakeLockHandler(const FakeLockHandler&) = delete;
  FakeLockHandler& operator=(const FakeLockHandler&) = delete;

  ~FakeLockHandler() override;

  // LockHandler:
  void ShowBannerMessage(const std::u16string& message,
                         bool is_warning) override;
  void ShowUserPodCustomIcon(
      const AccountId& account_id,
      const ScreenlockBridge::UserPodCustomIconInfo& icon_info) override;
  void HideUserPodCustomIcon(const AccountId& account_id) override;
  void SetSmartLockState(const AccountId& account_id,
                         ash::SmartLockState state) override;
  void NotifySmartLockAuthResult(const AccountId& account_id,
                                 bool successful) override;
  void EnableInput() override;
  void SetAuthType(const AccountId& account_id,
                   mojom::AuthType auth_type,
                   const std::u16string& auth_value) override;
  mojom::AuthType GetAuthType(const AccountId& account_id) const override;
  ScreenType GetScreenType() const override;
  void Unlock(const AccountId& account_id) override;
  void AttemptEasySignin(const AccountId& account_id,
                         const std::string& secret,
                         const std::string& key_label) override;
};

}  // namespace proximity_auth

#endif  // ASH_COMPONENTS_PROXIMITY_AUTH_FAKE_LOCK_HANDLER_H_
