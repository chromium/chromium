// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_enable_debugging_screen.h"

namespace ash {

MockEnableDebuggingScreen::MockEnableDebuggingScreen(
    base::WeakPtr<EnableDebuggingScreenView> view,
    const base::RepeatingClosure& exit_callback)
    : EnableDebuggingScreen(std::move(view), exit_callback) {}

MockEnableDebuggingScreen::~MockEnableDebuggingScreen() = default;

void MockEnableDebuggingScreen::ExitScreen() {
  exit_callback()->Run();
}

MockEnableDebuggingScreenView::MockEnableDebuggingScreenView() = default;

MockEnableDebuggingScreenView::~MockEnableDebuggingScreenView() = default;

}  // namespace ash
