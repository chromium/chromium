// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/mock_enrollment_screen.h"

namespace chromeos {

MockEnrollmentScreen::MockEnrollmentScreen(
    EnrollmentScreenView* view,
    const ScreenExitCallback& exit_callback)
    : EnrollmentScreen(view, exit_callback) {}

void MockEnrollmentScreen::ExitScreen(Result screen_result) {
  exit_callback()->Run(screen_result);
}

MockEnrollmentScreen::~MockEnrollmentScreen() {}

MockEnrollmentScreenView::MockEnrollmentScreenView() {}

MockEnrollmentScreenView::~MockEnrollmentScreenView() {}

}  // namespace chromeos
