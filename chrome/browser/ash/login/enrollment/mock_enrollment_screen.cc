// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"

namespace ash {

MockEnrollmentScreen::MockEnrollmentScreen(
    base::WeakPtr<EnrollmentScreenView> view,
    ErrorScreen* error_screen,
    const ScreenExitCallback& exit_callback)
    : EnrollmentScreen(std::move(view), error_screen, exit_callback) {}

void MockEnrollmentScreen::ExitScreen(Result screen_result) {
  exit_callback()->Run(screen_result);
}

MockEnrollmentScreen::~MockEnrollmentScreen() = default;

MockEnrollmentScreenView::MockEnrollmentScreenView() = default;

MockEnrollmentScreenView::~MockEnrollmentScreenView() = default;

}  // namespace ash
