// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_demo_preferences_screen.h"

namespace chromeos {

MockDemoPreferencesScreen::MockDemoPreferencesScreen(
    DemoPreferencesScreenView* view,
    const ScreenExitCallback& exit_callback)
    : DemoPreferencesScreen(view, exit_callback) {}

MockDemoPreferencesScreen::~MockDemoPreferencesScreen() = default;

void MockDemoPreferencesScreen::ExitScreen(Result result) {
  exit_callback()->Run(result);
}

MockDemoPreferencesScreenView::MockDemoPreferencesScreenView() = default;

MockDemoPreferencesScreenView::~MockDemoPreferencesScreenView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockDemoPreferencesScreenView::Bind(DemoPreferencesScreen* screen) {
  screen_ = screen;
  MockBind(screen);
}

}  // namespace chromeos
