// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_welcome_screen.h"

namespace chromeos {

MockWelcomeScreen::MockWelcomeScreen(
    WelcomeView* view,
    const WelcomeScreen::ScreenExitCallback& exit_callback)
    : WelcomeScreen(view, exit_callback) {}

void MockWelcomeScreen::ExitScreen(Result result) {
  exit_callback()->Run(result);
}

MockWelcomeScreen::~MockWelcomeScreen() = default;

MockWelcomeView::MockWelcomeView() = default;

MockWelcomeView::~MockWelcomeView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockWelcomeView::Bind(WelcomeScreen* screen) {
  screen_ = screen;
  MockBind(screen);
}

void MockWelcomeView::Unbind() {
  screen_ = nullptr;
  MockUnbind();
}

}  // namespace chromeos
