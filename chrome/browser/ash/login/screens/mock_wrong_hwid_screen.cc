// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_wrong_hwid_screen.h"

namespace chromeos {

MockWrongHWIDScreen::MockWrongHWIDScreen(
    WrongHWIDScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : WrongHWIDScreen(view, exit_callback) {}

MockWrongHWIDScreen::~MockWrongHWIDScreen() {}

void MockWrongHWIDScreen::ExitScreen() {
  WrongHWIDScreen::OnExit();
}

MockWrongHWIDScreenView::MockWrongHWIDScreenView() = default;

MockWrongHWIDScreenView::~MockWrongHWIDScreenView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockWrongHWIDScreenView::Bind(WrongHWIDScreen* screen) {
  screen_ = screen;
  MockBind(screen_);
}

void MockWrongHWIDScreenView::Unbind() {
  screen_ = nullptr;
  MockUnbind();
}

}  // namespace chromeos
