// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_WELCOME_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_WELCOME_SCREEN_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/welcome_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockWelcomeScreen : public WelcomeScreen {
 public:
  MockWelcomeScreen(WelcomeView* view,
                    const base::RepeatingClosure& exit_callback);
  ~MockWelcomeScreen() override;

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD1(SetConfiguration, void(base::Value* configuration));

  void ExitScreen();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWelcomeScreen);
};

class MockWelcomeView : public WelcomeView {
 public:
  MockWelcomeView();
  ~MockWelcomeView() override;

  void Bind(WelcomeScreen* screen) override;
  void Unbind() override;

  MOCK_METHOD1(MockBind, void(WelcomeScreen* screen));
  MOCK_METHOD0(MockUnbind, void());
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(StopDemoModeDetection, void());
  MOCK_METHOD0(ReloadLocalizedContent, void());
  MOCK_METHOD1(SetInputMethodId, void(const std::string& input_method_id));
  MOCK_METHOD1(SetTimezoneId, void(const std::string& timezone_id));

 private:
  WelcomeScreen* screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockWelcomeView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_WELCOME_SCREEN_H_
