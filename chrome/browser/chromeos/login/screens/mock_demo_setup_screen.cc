// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_demo_setup_screen.h"

namespace chromeos {

MockDemoSetupScreen::MockDemoSetupScreen(
    DemoSetupScreenView* view,
    const ScreenExitCallback& exit_callback)
    : DemoSetupScreen(view, exit_callback) {}

MockDemoSetupScreen::~MockDemoSetupScreen() = default;

void MockDemoSetupScreen::ExitScreen(Result result) {
  exit_callback()->Run(result);
}

MockDemoSetupScreenView::MockDemoSetupScreenView() = default;

MockDemoSetupScreenView::~MockDemoSetupScreenView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockDemoSetupScreenView::Bind(DemoSetupScreen* screen) {
  screen_ = screen;
  MockBind(screen);
}

}  // namespace chromeos
