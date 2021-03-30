// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_MOCK_SIGNIN_UI_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_MOCK_SIGNIN_UI_H_

#include "base/macros.h"
#include "chrome/browser/ash/login/ui/signin_ui.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockSigninUI : public SigninUI {
 public:
  MockSigninUI();
  virtual ~MockSigninUI();
  MockSigninUI(const MockSigninUI&) = delete;
  MockSigninUI& operator=(const SigninUI&) = delete;

  MOCK_METHOD(void, StartUserOnboarding, (), (override));
  MOCK_METHOD(void, StartSupervisionTransition, (), (override));
  MOCK_METHOD(void,
              StartEncryptionMigration,
              (const UserContext&,
               EncryptionMigrationMode,
               base::OnceCallback<void(const UserContext&)>),
              (override));
  MOCK_METHOD(void,
              SetAuthSessionForOnboarding,
              (const UserContext&),
              (override));
  MOCK_METHOD(void,
              ShowPasswordChangedDialog,
              (const AccountId&, bool),
              (override));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_MOCK_SIGNIN_UI_H_
