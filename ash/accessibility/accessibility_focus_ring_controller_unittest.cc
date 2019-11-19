// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_focus_ring_controller_impl.h"

#include "ash/accessibility/accessibility_cursor_ring_layer.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace ash {

using AccessibilityFocusRingControllerTest = AshTestBase;

TEST_F(AccessibilityFocusRingControllerTest, CallingHideWhenEmpty) {
  auto* controller = Shell::Get()->accessibility_focus_ring_controller();
  // Ensure that calling hide does not crash the controller if there are
  // no focus rings yet for a given ID.
  controller->HideFocusRing("catsRCute");
}

TEST_F(AccessibilityFocusRingControllerTest, SetFocusRingCorrectRingGroup) {
  auto* controller = Shell::Get()->accessibility_focus_ring_controller();
  EXPECT_EQ(nullptr, controller->GetFocusRingGroupForTesting("catsRCute"));
  SkColor cat_color = SkColorSetARGB(0xFF, 0x42, 0x42, 0x42);

  auto cat_focus_ring = std::make_unique<AccessibilityFocusRingInfo>();
  cat_focus_ring->color = cat_color;
  cat_focus_ring->rects_in_screen.push_back(gfx::Rect(0, 0, 10, 10));
  controller->SetFocusRing("catsRCute", std::move(cat_focus_ring));

  // A focus ring group was created.
  ASSERT_NE(nullptr, controller->GetFocusRingGroupForTesting("catsRCute"));
  EXPECT_EQ(1u, controller->GetFocusRingGroupForTesting("catsRCute")
                    ->focus_layers_for_testing()
                    .size());

  EXPECT_EQ(nullptr, controller->GetFocusRingGroupForTesting("dogsRCool"));
  auto dog_focus_ring = std::make_unique<AccessibilityFocusRingInfo>();
  dog_focus_ring->rects_in_screen.push_back(gfx::Rect(10, 30, 70, 150));
  controller->SetFocusRing("dogsRCool", std::move(dog_focus_ring));

  ASSERT_NE(nullptr, controller->GetFocusRingGroupForTesting("dogsRCool"));
  int size = controller->GetFocusRingGroupForTesting("dogsRCool")
                 ->focus_layers_for_testing()
                 .size();
  EXPECT_EQ(1, size);
  // The first focus ring group was not updated.
  const std::vector<std::unique_ptr<AccessibilityFocusRingLayer>>& layers =
      controller->GetFocusRingGroupForTesting("catsRCute")
          ->focus_layers_for_testing();
  EXPECT_EQ(1u, layers.size());
  EXPECT_EQ(cat_color, layers[0]->color_for_testing());
}

TEST_F(AccessibilityFocusRingControllerTest, CursorWorksOnMultipleDisplays) {
  UpdateDisplay("400x400,500x500");
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  // Simulate a mouse event on the primary display.
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  gfx::Point location(90, 90);
  controller->SetCursorRing(location);
  AccessibilityCursorRingLayer* cursor_layer =
      controller->cursor_layer_for_testing();
  EXPECT_EQ(root_windows[0], cursor_layer->root_window());
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().x() - location.x()),
            50);
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().y() - location.y()),
            50);

  // Simulate a mouse event at the same local location on the secondary display.
  gfx::Point location_on_secondary = location;
  location_on_secondary.Offset(400, 0);
  controller->SetCursorRing(location_on_secondary);

  cursor_layer = controller->cursor_layer_for_testing();
  EXPECT_EQ(root_windows[1], cursor_layer->root_window());
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().x() - location.x()),
            50);
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().y() - location.y()),
            50);
}

TEST_F(AccessibilityFocusRingControllerTest, HighlightColorCalculation) {
  SkColor without_alpha = SkColorSetARGB(0xFF, 0x42, 0x42, 0x42);
  SkColor with_alpha = SkColorSetARGB(0x3D, 0x14, 0x15, 0x92);

  float default_opacity = 0.3f;
  SkColor result_color = SK_ColorWHITE;
  float result_opacity = 0.0f;

  AccessibilityFocusRingControllerImpl::GetColorAndOpacityFromColor(
      without_alpha, default_opacity, &result_color, &result_opacity);
  EXPECT_EQ(default_opacity, result_opacity);

  AccessibilityFocusRingControllerImpl::GetColorAndOpacityFromColor(
      with_alpha, default_opacity, &result_color, &result_opacity);
  EXPECT_NEAR(0.239f, result_opacity, .001);
}

}  // namespace ash
