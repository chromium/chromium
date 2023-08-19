// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_TEST_UTIL_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_TEST_UTIL_H_

#include "ash/ash_export.h"
#include "ash/wm/window_state.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// Matchers --------------------------------------------------------------------

MATCHER_P(Minimized, matcher, "") {
  return testing::Matches(matcher)(WindowState::Get(arg.get())->IsMinimized());
}

// Utilities -------------------------------------------------------------------

// Expects that scrims `exist` on all root windows with expected properties.
ASH_EXPORT void ExpectScrimsOnAllRootWindows(bool exist);

// Returns the Welcome Tour dialog's accept button. If the dialog does not
// exist, returns `nullptr`.
ASH_EXPORT const views::View* GetDialogAcceptButton();

// Returns the Welcome Tour dialog's cancel button. If the dialog does not
// exist, returns `nullptr`.
ASH_EXPORT const views::View* GetDialogCancelButton();

// Waits for a window to be minimized. If the window is not minimized before the
// timeout, returns false.
ASH_EXPORT bool WaitUntilMinimized(aura::Window* window);

}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_TEST_UTIL_H_
