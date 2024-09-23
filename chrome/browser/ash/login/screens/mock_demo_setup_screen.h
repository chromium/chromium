// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_DEMO_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_DEMO_SETUP_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/screens/demo_setup_screen.h"
#include "chrome/browser/ui/webui/ash/login/demo_setup_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockDemoSetupScreen : public DemoSetupScreen {
 public:
  MockDemoSetupScreen(base::WeakPtr<DemoSetupScreenView> view,
                      const ScreenExitCallback& exit_callback);
  ~MockDemoSetupScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen(Result result);
};

class MockDemoSetupScreenView : public DemoSetupScreenView {
 public:
  MockDemoSetupScreenView();
  ~MockDemoSetupScreenView() override;

  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, OnSetupSucceeded, ());
  MOCK_METHOD(void,
              OnSetupFailed,
              (const DemoSetupController::DemoSetupError& error));
  MOCK_METHOD(void,
              SetCurrentSetupStep,
              (const DemoSetupController::DemoSetupStep current_step));

  base::WeakPtr<DemoSetupScreenView> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<DemoSetupScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_DEMO_SETUP_SCREEN_H_
