// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_test_util.h"

#include <string>
#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider_source.h"
#include "ash/user_education/welcome_tour/welcome_tour_scrim.h"
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

}  // namespace

// Utilities -------------------------------------------------------------------

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

}  // namespace ash
