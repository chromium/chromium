// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"

#include "ash/accessibility/ui/accessibility_cursor_ring_layer.h"
#include "ash/accessibility/ui/accessibility_highlight_layer.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_textured.h"
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

TEST_F(AccessibilityFocusRingControllerTest, FocusRingParentContainer) {
  auto* controller = Shell::Get()->accessibility_focus_ring_controller();
  auto focus_ring = std::make_unique<AccessibilityFocusRingInfo>();
  focus_ring->rects_in_screen.push_back(gfx::Rect(10, 10, 50, 50));
  controller->SetFocusRing("test_ring", std::move(focus_ring));

  // Default stacking order is ABOVE_ACCESSIBILITY_BUBBLES.
  ASSERT_NE(nullptr, controller->GetFocusRingGroupForTesting("test_ring"));
  const std::vector<std::unique_ptr<AccessibilityFocusRingLayer>>& layers =
      controller->GetFocusRingGroupForTesting("test_ring")
          ->focus_layers_for_testing();
  EXPECT_EQ(1u, layers.size());
  aura::Window* overlay_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_OverlayContainer);
  EXPECT_EQ(overlay_container, layers[0]->root_window());
  EXPECT_EQ(overlay_container->layer(), layers[0]->layer()->parent());
}

TEST_F(AccessibilityFocusRingControllerTest, DynamicStackingOrderReparenting) {
  auto* controller = Shell::Get()->accessibility_focus_ring_controller();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* overlay_container =
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);
  aura::Window* bubble_container = Shell::GetContainer(
      root_window, kShellWindowId_AccessibilityBubbleContainer);

  auto ring_above = std::make_unique<AccessibilityFocusRingInfo>();
  ring_above->rects_in_screen.push_back(gfx::Rect(10, 10, 50, 50));
  ring_above->stacking_order =
      FocusRingStackingOrder::ABOVE_ACCESSIBILITY_BUBBLES;
  controller->SetFocusRing("dynamic_caller", std::move(ring_above));

  const auto& layers = controller->GetFocusRingGroupForTesting("dynamic_caller")
                           ->focus_layers_for_testing();
  ASSERT_EQ(1u, layers.size());
  EXPECT_EQ(overlay_container, layers[0]->root_window());
  EXPECT_EQ(overlay_container->layer(), layers[0]->layer()->parent());

  auto ring_below = std::make_unique<AccessibilityFocusRingInfo>();
  ring_below->rects_in_screen.push_back(gfx::Rect(20, 20, 50, 50));
  ring_below->stacking_order =
      FocusRingStackingOrder::BELOW_ACCESSIBILITY_BUBBLES;
  controller->SetFocusRing("dynamic_caller", std::move(ring_below));

  EXPECT_EQ(bubble_container, layers[0]->root_window());
  EXPECT_EQ(bubble_container->layer(), layers[0]->layer()->parent());

  auto ring_above_again = std::make_unique<AccessibilityFocusRingInfo>();
  ring_above_again->rects_in_screen.push_back(gfx::Rect(30, 30, 50, 50));
  ring_above_again->stacking_order =
      FocusRingStackingOrder::ABOVE_ACCESSIBILITY_BUBBLES;
  controller->SetFocusRing("dynamic_caller", std::move(ring_above_again));

  EXPECT_EQ(overlay_container, layers[0]->root_window());
  EXPECT_EQ(overlay_container->layer(), layers[0]->layer()->parent());
}

TEST_F(AccessibilityFocusRingControllerTest,
       DynamicStackingOrderReparentingPersistMode) {
  auto* controller = Shell::Get()->accessibility_focus_ring_controller();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* overlay_container =
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);
  aura::Window* bubble_container = Shell::GetContainer(
      root_window, kShellWindowId_AccessibilityBubbleContainer);

  // ChromeVox and Select-to-Speak use PERSIST; verify reparenting works while
  // focus_layers_[0] animates rather than via the direct Set() path.
  auto ring_above = std::make_unique<AccessibilityFocusRingInfo>();
  ring_above->behavior = FocusRingBehavior::PERSIST;
  ring_above->rects_in_screen.push_back(gfx::Rect(10, 10, 50, 50));
  ring_above->stacking_order =
      FocusRingStackingOrder::ABOVE_ACCESSIBILITY_BUBBLES;
  controller->SetFocusRing("persist_caller", std::move(ring_above));

  const auto& layers = controller->GetFocusRingGroupForTesting("persist_caller")
                           ->focus_layers_for_testing();
  ASSERT_EQ(1u, layers.size());
  EXPECT_EQ(overlay_container, layers[0]->root_window());
  EXPECT_EQ(overlay_container->layer(), layers[0]->layer()->parent());

  auto ring_below = std::make_unique<AccessibilityFocusRingInfo>();
  ring_below->behavior = FocusRingBehavior::PERSIST;
  ring_below->rects_in_screen.push_back(gfx::Rect(20, 20, 50, 50));
  ring_below->stacking_order =
      FocusRingStackingOrder::BELOW_ACCESSIBILITY_BUBBLES;
  controller->SetFocusRing("persist_caller", std::move(ring_below));

  EXPECT_EQ(bubble_container, layers[0]->root_window());
  EXPECT_EQ(bubble_container->layer(), layers[0]->layer()->parent());

  auto ring_above_again = std::make_unique<AccessibilityFocusRingInfo>();
  ring_above_again->behavior = FocusRingBehavior::PERSIST;
  ring_above_again->rects_in_screen.push_back(gfx::Rect(30, 30, 50, 50));
  ring_above_again->stacking_order =
      FocusRingStackingOrder::ABOVE_ACCESSIBILITY_BUBBLES;
  controller->SetFocusRing("persist_caller", std::move(ring_above_again));

  EXPECT_EQ(overlay_container, layers[0]->root_window());
  EXPECT_EQ(overlay_container->layer(), layers[0]->layer()->parent());
}

TEST_F(AccessibilityFocusRingControllerTest,
       AboveAccessibilityBubblesStacksAboveOverlaySibling) {
  auto* controller = Shell::Get()->accessibility_focus_ring_controller();
  aura::Window* overlay_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_OverlayContainer);

  // Add a sibling layer simulating another widget in OverlayContainer.
  ui::LayerTextured sibling_overlay_layer;
  overlay_container->layer()->Add(&sibling_overlay_layer);
  overlay_container->layer()->StackAtTop(&sibling_overlay_layer);

  auto focus_ring = std::make_unique<AccessibilityFocusRingInfo>();
  focus_ring->behavior = FocusRingBehavior::PERSIST;
  focus_ring->rects_in_screen.push_back(gfx::Rect(10, 10, 50, 50));
  focus_ring->stacking_order =
      FocusRingStackingOrder::ABOVE_ACCESSIBILITY_BUBBLES;
  controller->SetFocusRing("above_caller", std::move(focus_ring));

  const auto& layers = controller->GetFocusRingGroupForTesting("above_caller")
                           ->focus_layers_for_testing();
  ASSERT_EQ(1u, layers.size());
  const auto& children = overlay_container->layer()->children();
  ASSERT_FALSE(children.empty());
  EXPECT_EQ(layers[0]->layer(), children.back());

  // Simulate another overlay widget (e.g. Alt-Tab window cycle list) stacking
  // on top, then verify the next focus update restacks the focus ring on top.
  overlay_container->layer()->StackAtTop(&sibling_overlay_layer);
  EXPECT_EQ(&sibling_overlay_layer, children.back());

  auto next_focus_ring = std::make_unique<AccessibilityFocusRingInfo>();
  next_focus_ring->behavior = FocusRingBehavior::PERSIST;
  next_focus_ring->rects_in_screen.push_back(gfx::Rect(20, 20, 50, 50));
  next_focus_ring->stacking_order =
      FocusRingStackingOrder::ABOVE_ACCESSIBILITY_BUBBLES;
  controller->SetFocusRing("above_caller", std::move(next_focus_ring));
  EXPECT_EQ(layers[0]->layer(), children.back());
  overlay_container->layer()->Remove(&sibling_overlay_layer);
}

TEST_F(AccessibilityFocusRingControllerTest,
       BelowAccessibilityBubblesStacksBelowBubbleSibling) {
  auto* controller = Shell::Get()->accessibility_focus_ring_controller();
  aura::Window* bubble_container =
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_AccessibilityBubbleContainer);

  // Add a sibling layer simulating an accessibility bubble in the container.
  ui::LayerTextured sibling_bubble_layer;
  bubble_container->layer()->Add(&sibling_bubble_layer);
  bubble_container->layer()->StackAtTop(&sibling_bubble_layer);

  auto focus_ring = std::make_unique<AccessibilityFocusRingInfo>();
  focus_ring->behavior = FocusRingBehavior::PERSIST;
  focus_ring->rects_in_screen.push_back(gfx::Rect(10, 10, 50, 50));
  focus_ring->stacking_order =
      FocusRingStackingOrder::BELOW_ACCESSIBILITY_BUBBLES;
  controller->SetFocusRing("below_caller", std::move(focus_ring));

  const auto& layers = controller->GetFocusRingGroupForTesting("below_caller")
                           ->focus_layers_for_testing();
  ASSERT_EQ(1u, layers.size());
  const auto& children = bubble_container->layer()->children();
  ASSERT_FALSE(children.empty());
  EXPECT_EQ(layers[0]->layer(), children.front());

  // Simulate a bubble sibling stacking at the bottom, then verify the next
  // focus update restacks the focus ring at the bottom.
  bubble_container->layer()->StackAtBottom(&sibling_bubble_layer);
  EXPECT_EQ(&sibling_bubble_layer, children.front());

  auto next_focus_ring = std::make_unique<AccessibilityFocusRingInfo>();
  next_focus_ring->behavior = FocusRingBehavior::PERSIST;
  next_focus_ring->rects_in_screen.push_back(gfx::Rect(20, 20, 50, 50));
  next_focus_ring->stacking_order =
      FocusRingStackingOrder::BELOW_ACCESSIBILITY_BUBBLES;
  controller->SetFocusRing("below_caller", std::move(next_focus_ring));
  EXPECT_EQ(layers[0]->layer(), children.front());
  bubble_container->layer()->Remove(&sibling_bubble_layer);
}

// Disabled due to failure. http://crbug.com/1279278
TEST_F(AccessibilityFocusRingControllerTest, DISABLED_SetFocusRingCorrectRingGroup) {
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
  // Set a background color to ensure that code path has no surprises.
  dog_focus_ring->background_color = SK_ColorBLUE;
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
  UpdateDisplay("500x400,500x400");
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  // Simulate a mouse event on the primary display.
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  gfx::Point location(90, 90);
  controller->SetCursorRing(location);
  AccessibilityCursorRingLayer* cursor_layer =
      controller->cursor_layer_for_testing();
  aura::Window* window0_container = Shell::GetContainer(
      root_windows[0], kShellWindowId_AccessibilityBubbleContainer);
  EXPECT_EQ(window0_container, cursor_layer->root_window());
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().x() - location.x()),
            50);
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().y() - location.y()),
            50);

  // Simulate a mouse event at the same local location on the secondary display.
  gfx::Point location_on_secondary = location;
  location_on_secondary.Offset(500, 0);
  controller->SetCursorRing(location_on_secondary);

  cursor_layer = controller->cursor_layer_for_testing();
  aura::Window* window1_container = Shell::GetContainer(
      root_windows[1], kShellWindowId_AccessibilityBubbleContainer);
  EXPECT_EQ(window1_container, cursor_layer->root_window());
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().x() - location.x()),
            50);
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().y() - location.y()),
            50);
}

// Disabled due to failure. http://crbug.com/1279278
TEST_F(AccessibilityFocusRingControllerTest, DISABLED_FocusRingWorksOnMultipleDisplays) {
  UpdateDisplay("500x400,600x500");
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();

  auto focus_ring = std::make_unique<AccessibilityFocusRingInfo>();
  focus_ring->color = SkColorSetRGB(0x33, 0x66, 0x99);
  focus_ring->rects_in_screen.push_back(gfx::Rect(50, 50, 10, 10));
  controller->SetFocusRing("catsRCute", std::move(focus_ring));

  // A focus ring group was created.
  ASSERT_NE(nullptr, controller->GetFocusRingGroupForTesting("catsRCute"));
  const std::vector<std::unique_ptr<AccessibilityFocusRingLayer>>& layers =
      controller->GetFocusRingGroupForTesting("catsRCute")
          ->focus_layers_for_testing();
  EXPECT_EQ(1u, layers.size());
  aura::Window* window0_container =
      Shell::GetContainer(root_windows[0], kShellWindowId_OverlayContainer);
  EXPECT_EQ(window0_container, layers[0]->root_window());
  // The focus ring has some padding, so just check the center point is where
  // we would expect it.
  EXPECT_EQ(layers[0]->layer()->GetTargetBounds().CenterPoint(),
            gfx::Rect(50, 50, 10, 10).CenterPoint());

  // Move it to the secondary display.
  auto moved = std::make_unique<AccessibilityFocusRingInfo>();
  moved->color = SkColorSetRGB(0x33, 0x66, 0x99);
  moved->rects_in_screen.push_back(gfx::Rect(600, 50, 10, 10));
  controller->SetFocusRing("catsRCute", std::move(moved));

  // Check it is correctly positioned on the secondary display.
  const std::vector<std::unique_ptr<AccessibilityFocusRingLayer>>&
      moved_layers = controller->GetFocusRingGroupForTesting("catsRCute")
                         ->focus_layers_for_testing();
  EXPECT_EQ(1u, moved_layers.size());
  aura::Window* window1_container =
      Shell::GetContainer(root_windows[1], kShellWindowId_OverlayContainer);
  EXPECT_EQ(window1_container, moved_layers[0]->root_window());
  EXPECT_EQ(moved_layers[0]->layer()->GetTargetBounds().CenterPoint(),
            gfx::Rect(100, 50, 10, 10).CenterPoint());
}

TEST_F(AccessibilityFocusRingControllerTest, HighlightWorksOnMultipleDisplays) {
  UpdateDisplay("500x400,600x500");
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  aura::Window* window0_container = Shell::GetContainer(
      root_windows[0], kShellWindowId_AccessibilityPanelContainer);
  aura::Window* window1_container = Shell::GetContainer(
      root_windows[1], kShellWindowId_AccessibilityPanelContainer);

  // Simulate highlighting on the primary display.
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  controller->SetHighlights(
      {gfx::Rect(50, 50, 10, 10), gfx::Rect(50, 60, 10, 10)},
      SkColorSetRGB(0x33, 0x66, 0x99));

  AccessibilityHighlightLayer* highlight_layer =
      controller->highlight_layer_for_testing();
  EXPECT_EQ(window0_container, highlight_layer->root_window());
  // The highlight bounds has some padding, so just check the center point is
  // where we would expect it.
  EXPECT_EQ(highlight_layer->layer()->GetTargetBounds().CenterPoint(),
            gfx::Rect(50, 50, 10, 20).CenterPoint());
  EXPECT_EQ(std::vector<gfx::Rect>(
                {gfx::Rect(50, 50, 10, 10), gfx::Rect(50, 60, 10, 10)}),
            highlight_layer->rects_for_test());

  // Simulate highlighting on the secondary display.
  controller->SetHighlights(
      {gfx::Rect(550, 50, 10, 10), gfx::Rect(550, 60, 10, 10)},
      SkColorSetRGB(0x33, 0x66, 0x99));

  highlight_layer = controller->highlight_layer_for_testing();
  EXPECT_EQ(window1_container, highlight_layer->root_window());
  EXPECT_EQ(highlight_layer->layer()->GetTargetBounds().CenterPoint(),
            gfx::Rect(50, 50, 10, 20).CenterPoint());
  EXPECT_EQ(std::vector<gfx::Rect>(
                {gfx::Rect(50, 50, 10, 10), gfx::Rect(50, 60, 10, 10)}),
            highlight_layer->rects_for_test());
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
