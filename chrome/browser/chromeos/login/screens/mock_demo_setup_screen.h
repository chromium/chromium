// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_DEMO_SETUP_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_DEMO_SETUP_SCREEN_H_

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/screens/demo_setup_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_setup_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockDemoSetupScreen : public DemoSetupScreen {
 public:
  MockDemoSetupScreen(DemoSetupScreenView* view,
                      const ScreenExitCallback& exit_callback);
  ~MockDemoSetupScreen() override;

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());

  void ExitScreen(Result result);
};

class MockDemoSetupScreenView : public DemoSetupScreenView {
 public:
  MockDemoSetupScreenView();
  ~MockDemoSetupScreenView() override;

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD1(MockBind, void(DemoSetupScreen* screen));
  MOCK_METHOD0(OnSetupSucceeded, void());
  MOCK_METHOD1(OnSetupFailed,
               void(const DemoSetupController::DemoSetupError& error));

  void Bind(DemoSetupScreen* screen) override;

 private:
  DemoSetupScreen* screen_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_DEMO_SETUP_SCREEN_H_
