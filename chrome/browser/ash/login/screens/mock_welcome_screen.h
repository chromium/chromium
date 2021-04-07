// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_WELCOME_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_WELCOME_SCREEN_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockWelcomeScreen : public WelcomeScreen {
 public:
  MockWelcomeScreen(WelcomeView* view,
                    const WelcomeScreen::ScreenExitCallback& exit_callback);
  ~MockWelcomeScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen(Result result);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWelcomeScreen);
};

class MockWelcomeView : public WelcomeView {
 public:
  MockWelcomeView();
  ~MockWelcomeView() override;

  void Bind(WelcomeScreen* screen) override;
  void Unbind() override;

  MOCK_METHOD(void, MockBind, (WelcomeScreen * screen));
  MOCK_METHOD(void, MockUnbind, ());
  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, ReloadLocalizedContent, ());
  MOCK_METHOD(void, SetInputMethodId, (const std::string& input_method_id));
  MOCK_METHOD(void, SetTimezoneId, (const std::string& timezone_id));
  MOCK_METHOD(void, ShowDemoModeConfirmationDialog, ());
  MOCK_METHOD(void, ShowEditRequisitionDialog, (const std::string&));
  MOCK_METHOD(void, ShowRemoraRequisitionDialog, ());
  MOCK_METHOD(void, GiveChromeVoxHint, ());
  MOCK_METHOD(void, CancelChromeVoxHintIdleDetection, ());

 private:
  WelcomeScreen* screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockWelcomeView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_WELCOME_SCREEN_H_
