// Copyright 2014 The Chromium Authors
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

  ~MockLoginDisplay() override;

  MOCK_METHOD(void, Init, (const user_manager::UserList&, bool), (override));
  MOCK_METHOD(void, SetUIEnabled, (bool), (override));
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_MOCK_LOGIN_DISPLAY_H_
