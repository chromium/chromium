// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_MOCK_LOCK_HANDLER_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_MOCK_LOCK_HANDLER_H_

#include <string>

#include "chromeos/ash/components/proximity_auth/public/mojom/auth_type.mojom-shared.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// Mock implementation of proximity_auth::ScreenlockBridge::LockHandler.
class MockLockHandler : public proximity_auth::ScreenlockBridge::LockHandler {
 public:
  MockLockHandler();
  ~MockLockHandler() override;

  // proximity_auth::ScreenlockBridge::LockHandler:
  MOCK_METHOD(void,
              ShowBannerMessage,
              (const std::u16string& message, bool is_warning));
  MOCK_METHOD(void,
              SetSmartLockState,
              (const AccountId& account_id, SmartLockState statel));
  MOCK_METHOD(void,
              NotifySmartLockAuthResult,
              (const AccountId& account_id, bool successful));
  MOCK_METHOD(void, EnableInput, ());
  MOCK_METHOD(void,
              SetAuthType,
              (const AccountId& account_id,
               proximity_auth::mojom::AuthType auth_type,
               const std::u16string& auth_value));
  MOCK_METHOD(proximity_auth::mojom::AuthType,
              GetAuthType,
              (const AccountId& account_id),
              (const));
  MOCK_METHOD(ScreenType, GetScreenType, (), (const));
  MOCK_METHOD(void, Unlock, (const AccountId& account_id));
  MOCK_METHOD(void,
              AttemptEasySignin,
              (const AccountId& account_id,
               const std::string& secret,
               const std::string& key_label));
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_MOCK_LOCK_HANDLER_H_
