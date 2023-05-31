// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_scrim.h"

#include <string>

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/welcome_tour/welcome_tour_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider.h"

namespace ash {

// WelcomeTourScrimTest --------------------------------------------------------

// Base class for tests of the `WelcomeTourScrim`.
using WelcomeTourScrimTest = UserEducationAshTestBase;

// Tests -----------------------------------------------------------------------

// Verifies that scrims take effect only while `WelcomeTourScrim` is in scope.
TEST_F(WelcomeTourScrimTest, Scope) {
  // Initial state.
  ExpectScrimsOnAllRootWindows(false);

  {
    // Case: In scope.
    WelcomeTourScrim scrim;
    ExpectScrimsOnAllRootWindows(true);
  }

  // Case: Out of scope.
  ExpectScrimsOnAllRootWindows(false);
}

// Verifies that root windows can be added/removed while `WelcomeTourScrim` is
// in scope and that there will be properly configured scrims on all root
// windows.
TEST_F(WelcomeTourScrimTest, AddRemoveRootWindows) {
  // Initial state.
  WelcomeTourScrim scrim;
  ExpectScrimsOnAllRootWindows(true);

  // Case: Add root window.
  UpdateDisplay("1024x768,1024x768");
  ExpectScrimsOnAllRootWindows(true);

  // Case: Remove root window.
  UpdateDisplay("1024x768");
  ExpectScrimsOnAllRootWindows(true);
}

// Verifies that root windows can be resized while `WelcomeTourScrim` is in
// scope and that there will be properly configured scrims on all root windows.
TEST_F(WelcomeTourScrimTest, ResizeRootWindows) {
  // Initial state.
  WelcomeTourScrim scrim;
  ExpectScrimsOnAllRootWindows(true);

  // Case: Shrink root window.
  UpdateDisplay("360x640");
  ExpectScrimsOnAllRootWindows(true);

  // Case: Grow root window.
  UpdateDisplay("1920x1080");
  ExpectScrimsOnAllRootWindows(true);
}

// Verifies that a root window's color provider can be updated while
// `WelcomeTourScrim` is in scope and that there will be properly configured
// scrims on all root windows.
TEST_F(WelcomeTourScrimTest, UpdateRootWindowColorProvider) {
  // Log in the user so that the prefs backing dark/light mode are available.
  SimulateUserLogin("user@test");

  // Initial state.
  WelcomeTourScrim scrim;
  ExpectScrimsOnAllRootWindows(true);

  // Case: Toggle dark/light mode.
  auto* const controller = DarkLightModeController::Get();
  controller->SetDarkModeEnabledForTest(!controller->IsDarkModeEnabled());
  ExpectScrimsOnAllRootWindows(true);
}

}  // namespace ash
