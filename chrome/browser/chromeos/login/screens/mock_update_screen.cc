// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_update_screen.h"

using ::testing::AtLeast;
using ::testing::_;

namespace chromeos {

MockUpdateScreen::MockUpdateScreen(
    UpdateView* view,
    ErrorScreen* error_screen,
    const UpdateScreen::ScreenExitCallback& exit_callback)
    : UpdateScreen(view, error_screen, exit_callback) {}

MockUpdateScreen::~MockUpdateScreen() {}

void MockUpdateScreen::RunExit(UpdateScreen::Result result) {
  ExitUpdate(result);
}

MockUpdateView::MockUpdateView() {
  EXPECT_CALL(*this, MockBind(_)).Times(AtLeast(1));
}

MockUpdateView::~MockUpdateView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockUpdateView::Bind(UpdateScreen* screen) {
  screen_ = screen;
  MockBind(screen);
}

void MockUpdateView::Unbind() {
  screen_ = nullptr;
  MockUnbind();
}

}  // namespace chromeos
