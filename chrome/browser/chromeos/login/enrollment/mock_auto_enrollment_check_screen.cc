// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/mock_auto_enrollment_check_screen.h"

namespace chromeos {

MockAutoEnrollmentCheckScreen::MockAutoEnrollmentCheckScreen(
    AutoEnrollmentCheckScreenView* view,
    ErrorScreen* error_screen,
    const base::RepeatingClosure& exit_callback)
    : AutoEnrollmentCheckScreen(view, error_screen, exit_callback) {}

MockAutoEnrollmentCheckScreen::~MockAutoEnrollmentCheckScreen() {}

void MockAutoEnrollmentCheckScreen::RealShow() {
  AutoEnrollmentCheckScreen::Show();
}

void MockAutoEnrollmentCheckScreen::ExitScreen() {
  RunExitCallback();
}

MockAutoEnrollmentCheckScreenView::MockAutoEnrollmentCheckScreenView() =
    default;

MockAutoEnrollmentCheckScreenView::~MockAutoEnrollmentCheckScreenView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockAutoEnrollmentCheckScreenView::SetDelegate(Delegate* screen) {
  screen_ = screen;
  MockSetDelegate(screen);
}

}  // namespace chromeos
