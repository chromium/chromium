// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/mock_auto_enrollment_check_screen.h"

namespace ash {

MockAutoEnrollmentCheckScreen::MockAutoEnrollmentCheckScreen(
    PrefService* local_state,
    base::WeakPtr<AutoEnrollmentCheckScreenView> view,
    ErrorScreen* error_screen,
    const base::RepeatingCallback<void(Result result)>& exit_callback)
    : AutoEnrollmentCheckScreen(local_state,
                                std::move(view),
                                error_screen,
                                exit_callback) {}

MockAutoEnrollmentCheckScreen::~MockAutoEnrollmentCheckScreen() = default;

void MockAutoEnrollmentCheckScreen::RealShow() {
  AutoEnrollmentCheckScreen::ShowImpl();
}

void MockAutoEnrollmentCheckScreen::ExitScreen() {
  RunExitCallback(Result::NEXT);
}

MockAutoEnrollmentCheckScreenView::MockAutoEnrollmentCheckScreenView() =
    default;

MockAutoEnrollmentCheckScreenView::~MockAutoEnrollmentCheckScreenView() =
    default;

base::WeakPtr<AutoEnrollmentCheckScreenView>
MockAutoEnrollmentCheckScreenView::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
