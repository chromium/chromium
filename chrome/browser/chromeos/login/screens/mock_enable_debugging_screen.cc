// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_enable_debugging_screen.h"

namespace chromeos {

MockEnableDebuggingScreen::MockEnableDebuggingScreen(
    EnableDebuggingScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : EnableDebuggingScreen(view, exit_callback) {}

MockEnableDebuggingScreen::~MockEnableDebuggingScreen() {}

void MockEnableDebuggingScreen::ExitScreen() {
  exit_callback()->Run();
}

MockEnableDebuggingScreenView::MockEnableDebuggingScreenView() = default;

MockEnableDebuggingScreenView::~MockEnableDebuggingScreenView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockEnableDebuggingScreenView::SetDelegate(EnableDebuggingScreen* screen) {
  screen_ = screen;
  MockSetDelegate(screen_);
}

}  // namespace chromeos
