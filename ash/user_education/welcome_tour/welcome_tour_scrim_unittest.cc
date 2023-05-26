// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_scrim.h"

#include <string>
#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"

namespace ash {
namespace {

// Aliases.
using ::testing::Conditional;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Matches;
using ::testing::Not;

// Matchers --------------------------------------------------------------------

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

aura::Window* GetHelpBubbleContainer(RootWindowController* controller) {
  return controller->GetRootWindow()->GetChildById(
      kShellWindowId_HelpBubbleContainer);
}

void ExpectScrimsOnAllRootWindows(bool exist) {
  for (auto* controller : GetAllRootWindowControllers()) {
    auto* const help_bubble_container = GetHelpBubbleContainer(controller);
    EXPECT_THAT(
        help_bubble_container->layer()->children(),
        Conditional(exist, Index(0, Name(Eq(WelcomeTourScrim::kLayerName))),
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

}  // namespace ash
