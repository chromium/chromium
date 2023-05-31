// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_scrim.h"

#include <string>
#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider_source.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"

namespace ash {
namespace {

// Aliases.
using ::testing::AllOf;
using ::testing::Conditional;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Matches;
using ::testing::Not;

// Matchers --------------------------------------------------------------------

MATCHER_P(BackgroundColor, matcher, "") {
  return Matches(matcher)(arg->background_color());
}

MATCHER_P(Bounds, matcher, "") {
  return Matches(matcher)(arg->bounds());
}

MATCHER_P2(Index, i, matcher, "") {
  return Matches(matcher)(arg.at(i));
}

MATCHER_P(Name, matcher, "") {
  return Matches(matcher)(arg->name());
}

// Helpers ---------------------------------------------------------------------

std::vector<RootWindowController*> GetAllRootWindowControllers() {
  return Shell::Get()
      ->window_tree_host_manager()
      ->GetAllRootWindowControllers();
}

SkColor GetColor(RootWindowController* controller, ui::ColorId id) {
  return controller->color_provider_source()->GetColorProvider()->GetColor(id);
}

aura::Window* GetHelpBubbleContainer(RootWindowController* controller) {
  return controller->GetRootWindow()->GetChildById(
      kShellWindowId_HelpBubbleContainer);
}

void ExpectScrimsOnAllRootWindows(bool exist) {
  for (auto* controller : GetAllRootWindowControllers()) {
    auto* const help_bubble_container = GetHelpBubbleContainer(controller);
    EXPECT_THAT(
        help_bubble_container->layer()->children(),
        Conditional(
            exist,
            Index(0,
                  AllOf(Name(Eq(WelcomeTourScrim::kLayerName)),
                        BackgroundColor(
                            GetColor(controller, cros_tokens::kCrosSysScrim)),
                        Bounds(gfx::Rect(
                            /*origin=*/gfx::Point(),
                            /*size=*/help_bubble_container->bounds().size())))),
            Not(Contains(Name(Eq(WelcomeTourScrim::kLayerName))))));
  }
}

}  // namespace

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
