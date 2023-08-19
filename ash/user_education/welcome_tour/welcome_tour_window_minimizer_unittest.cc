// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_window_minimizer.h"

#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/welcome_tour/welcome_tour_test_util.h"
#include "ash/wm/window_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Aliases ---------------------------------------------------------------------

using ::testing::Eq;

}  // namespace

// WelcomeTourWindowMinimizerTest ----------------------------------------------

// Base class for tests of the `WelcomeTourWindowMinimizer`.
using WelcomeTourWindowMinimizerTest = UserEducationAshTestBase;

// Tests -----------------------------------------------------------------------

// Verifies that minimization only occurs when `WelcomeTourWindowMinimizer` is
// in scope.
TEST_F(WelcomeTourWindowMinimizerTest, Scope) {
  std::unique_ptr<aura::Window> window_1 = CreateAppWindow();
  std::unique_ptr<aura::Window> window_2;

  // Initial state. Windows should not be minimized by default.
  EXPECT_THAT(window_1, Minimized(Eq(false)));

  {
    // Case: In scope. Any previously existing windows should be minimized, as
    // well as any newly created ones.
    WelcomeTourWindowMinimizer minimizer;
    EXPECT_TRUE(WaitUntilMinimized(window_1.get()));

    window_2 = CreateAppWindow();
    EXPECT_TRUE(WaitUntilMinimized(window_2.get()));
  }

  // Case: Out of scope.
  EXPECT_THAT(window_1, Minimized(Eq(true)));
  EXPECT_THAT(window_2, Minimized(Eq(true)));

  // Case: Window created out of scope.
  auto window_3 = CreateAppWindow();
  EXPECT_THAT(window_3, Minimized(Eq(false)));
}

// Verifies that windows will be minimized on displays added after the minimizer
// is initialized.
TEST_F(WelcomeTourWindowMinimizerTest, NewDisplay) {
  WelcomeTourWindowMinimizer minimizer;

  UpdateDisplay("500x400,500x400");

  auto window = CreateAppWindow(GetSecondaryDisplay().bounds());
  EXPECT_TRUE(WaitUntilMinimized(window.get()));
}

// Verifies that if a window is unminimized through user action or
// programmatically, it is immediately reminimized.
TEST_F(WelcomeTourWindowMinimizerTest, SquelchUnminimization) {
  WelcomeTourWindowMinimizer minimizer;

  auto window = CreateAppWindow();
  EXPECT_TRUE(WaitUntilMinimized(window.get()));

  auto* state = WindowState::Get(window.get());
  ASSERT_TRUE(state);
  state->Unminimize();

  EXPECT_TRUE(WaitUntilMinimized(window.get()));
}

}  // namespace ash
