// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_welcome_screen.h"

namespace chromeos {

MockWelcomeScreen::MockWelcomeScreen(
    WelcomeView* view,
    const base::RepeatingClosure& exit_callback)
    : WelcomeScreen(view, exit_callback) {}

void MockWelcomeScreen::ExitScreen() {
  exit_callback()->Run();
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
