// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_MOCK_LOGIN_DISPLAY_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_MOCK_LOGIN_DISPLAY_H_

#include "chrome/browser/ash/login/ui/login_display.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockLoginDisplay : public LoginDisplay {
 public:
  MockLoginDisplay();

  MockLoginDisplay(const MockLoginDisplay&) = delete;
  MockLoginDisplay& operator=(const MockLoginDisplay&) = delete;

  ~MockLoginDisplay();

  MOCK_METHOD0(ClearAndEnablePassword, void(void));
  MOCK_METHOD4(Init, void(const user_manager::UserList&, bool, bool, bool));
  MOCK_METHOD0(OnPreferencesChanged, void(void));
  MOCK_METHOD1(OnUserImageChanged, void(const user_manager::User&));
  MOCK_METHOD1(SetUIEnabled, void(bool));
  MOCK_METHOD1(ShowSigninUI, void(const std::string&));
  MOCK_METHOD0(ShowAllowlistCheckFailedError, void(void));
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::MockLoginDisplay;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_MOCK_LOGIN_DISPLAY_H_
