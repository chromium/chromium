// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_enable_adb_sideloading_screen.h"

namespace chromeos {

using ::testing::_;
using ::testing::AtLeast;

MockEnableAdbSideloadingScreen::MockEnableAdbSideloadingScreen(
    EnableAdbSideloadingScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : EnableAdbSideloadingScreen(view, exit_callback) {}

MockEnableAdbSideloadingScreen::~MockEnableAdbSideloadingScreen() {}

void MockEnableAdbSideloadingScreen::ExitScreen() {
  exit_callback()->Run();
}

MockEnableAdbSideloadingScreenView::MockEnableAdbSideloadingScreenView() {}

MockEnableAdbSideloadingScreenView::~MockEnableAdbSideloadingScreenView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockEnableAdbSideloadingScreenView::Bind(
    EnableAdbSideloadingScreen* screen) {
  screen_ = screen;
  MockBind(screen);
}

void MockEnableAdbSideloadingScreenView::Unbind() {
  screen_ = nullptr;
  MockUnbind();
}

}  // namespace chromeos
