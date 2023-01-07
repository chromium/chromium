// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_demo_setup_screen.h"
#include "base/memory/weak_ptr.h"

namespace ash {

MockDemoSetupScreen::MockDemoSetupScreen(
    base::WeakPtr<DemoSetupScreenView> view,
    const ScreenExitCallback& exit_callback)
    : DemoSetupScreen(view, exit_callback) {}

MockDemoSetupScreen::~MockDemoSetupScreen() = default;

void MockDemoSetupScreen::ExitScreen(Result result) {
  exit_callback()->Run(result);
}

MockDemoSetupScreenView::MockDemoSetupScreenView() = default;

MockDemoSetupScreenView::~MockDemoSetupScreenView() = default;

}  // namespace ash
