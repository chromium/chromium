// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/mock_enrollment_screen.h"

namespace ash {

MockEnrollmentScreen::MockEnrollmentScreen(
    EnrollmentScreenView* view,
    const ScreenExitCallback& exit_callback)
    : EnrollmentScreen(view, exit_callback) {}

void MockEnrollmentScreen::ExitScreen(Result screen_result) {
  exit_callback()->Run(screen_result);
}

MockEnrollmentScreen::~MockEnrollmentScreen() = default;

MockEnrollmentScreenView::MockEnrollmentScreenView() = default;

MockEnrollmentScreenView::~MockEnrollmentScreenView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockEnrollmentScreenView::Bind(EnrollmentScreen* screen) {
  screen_ = screen;
  MockBind(screen);
}

void MockEnrollmentScreenView::Unbind() {
  screen_ = nullptr;
  MockUnbind();
}

}  // namespace ash
