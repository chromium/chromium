// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"

#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/game_dashboard/game_dashboard_widget.h"
#include "ash/public/cpp/arc_game_controls_flag.h"
#include "chrome/browser/ash/arc/input_overlay/test/game_controls_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "ui/aura/window.h"

namespace arc::input_overlay {

class DisplayOverlayControllerTest : public GameControlsTestBase {
 public:
  DisplayOverlayControllerTest() = default;
  ~DisplayOverlayControllerTest() override = default;

  void RemoveAllActions() {
    for (const auto& action : touch_injector_->actions()) {
      controller_->RemoveAction(action.get());
    }
  }

  void CheckWidgets(bool has_input_mapping_widget,
                    bool hint_visible,
                    bool has_editing_list_widget) {
    EXPECT_EQ(has_input_mapping_widget, !!controller_->input_mapping_widget_);
    if (has_input_mapping_widget) {
      EXPECT_EQ(hint_visible, controller_->input_mapping_widget_->IsVisible());
    }
    EXPECT_EQ(has_editing_list_widget, !!controller_->editing_list_widget_);
  }

  bool CanRewriteEvent() { return touch_injector_->can_rewrite_event_; }
};

TEST_F(DisplayOverlayControllerTest, TestDisableEnableFeature) {
  CheckWidgets(/*has_input_mapping_widget=*/true, /*hint_visible=*/true,
               /*has_editing_list_widget=*/false);
  EXPECT_TRUE(CanRewriteEvent());

  auto* window = touch_injector_->window();
  auto flags = window->GetProperty(ash::kArcGameControlsFlagsKey);

  // 1. Disable and enable GIO in `kView` mode. All the widgets shouldn't show
  // up when GIO is disabled.
  // Disable GIO.
  window->SetProperty(
      ash::kArcGameControlsFlagsKey,
      ash::game_dashboard_utils::UpdateFlag(
          flags, ash::ArcGameControlsFlag::kEnabled, /*enable_flag=*/false));
  CheckWidgets(/*has_input_mapping_widget=*/false,
               /*hint_visible=*/false, /*has_editing_list_widget=*/false);
  EXPECT_FALSE(CanRewriteEvent());
  // Enable GIO back.
  window->SetProperty(
      ash::kArcGameControlsFlagsKey,
      ash::game_dashboard_utils::UpdateFlag(
          flags, ash::ArcGameControlsFlag::kEnabled, /*enable_flag=*/true));
  CheckWidgets(/*has_input_mapping_widget=*/true, /*hint_visible=*/true,
               /*has_editing_list_widget=*/false);
  EXPECT_TRUE(CanRewriteEvent());

  // 2. Disable and enable GIO in `kEdit` mode. All the widgets shouldn't show
  // up when GIO is disabled.
  EnableDisplayMode(DisplayMode::kEdit);
  EXPECT_FALSE(CanRewriteEvent());
  CheckWidgets(/*has_input_mapping_widget=*/true, /*hint_visible=*/true,
               /*has_editing_list_widget=*/true);
  // Disable GIO.
  window->SetProperty(
      ash::kArcGameControlsFlagsKey,
      ash::game_dashboard_utils::UpdateFlag(
          flags, ash::ArcGameControlsFlag::kEnabled, /*enable_flag=*/false));
  CheckWidgets(/*has_input_mapping_widget=*/false, /*hint_visible=*/false,
               /*has_editing_list_widget=*/false);
  // Enable GIO and overlay is displayed as view mode.
  window->SetProperty(
      ash::kArcGameControlsFlagsKey,
      ash::game_dashboard_utils::UpdateFlag(
          flags, ash::ArcGameControlsFlag::kEnabled, /*enable_flag=*/true));
  CheckWidgets(/*has_input_mapping_widget=*/true, /*hint_visible=*/true,
               /*has_editing_list_widget=*/false);
  EXPECT_TRUE(CanRewriteEvent());
}

TEST_F(DisplayOverlayControllerTest, TestHideMappingHint) {
  CheckWidgets(/*has_input_mapping_widget=*/true, /*hint_visible=*/true,
               /*has_editing_list_widget=*/false);
  EXPECT_TRUE(CanRewriteEvent());

  // Hide the mapping hint. Mapping hint is hidden in `kView` mode and
  // showing up in `kEdit` mode.
  auto* window = touch_injector_->window();
  window->SetProperty(
      ash::kArcGameControlsFlagsKey,
      ash::game_dashboard_utils::UpdateFlag(
          window->GetProperty(ash::kArcGameControlsFlagsKey),
          ash::ArcGameControlsFlag::kHint, /*enable_flag=*/false));
  CheckWidgets(/*has_input_mapping_widget=*/true,
               /*hint_visible=*/false, /*has_editing_list_widget=*/false);
  EXPECT_TRUE(CanRewriteEvent());
  EnableDisplayMode(DisplayMode::kEdit);
  // `input_mapping_widget_` is always showing up in edit mode.
  CheckWidgets(/*has_input_mapping_widget=*/true, /*hint_visible=*/true,
               /*has_editing_list_widget=*/true);
  EXPECT_FALSE(CanRewriteEvent());
  EnableDisplayMode(DisplayMode::kView);
  CheckWidgets(/*has_input_mapping_widget=*/true,
               /*hint_visible=*/false, /*has_editing_list_widget=*/false);
  EXPECT_TRUE(CanRewriteEvent());
}

TEST_F(DisplayOverlayControllerTest, TestRemoveAllActions) {
  CheckWidgets(/*has_input_mapping_widget=*/true, /*hint_visible=*/true,
               /*has_editing_list_widget=*/false);
  EXPECT_TRUE(CanRewriteEvent());

  EnableDisplayMode(DisplayMode::kEdit);
  EXPECT_FALSE(CanRewriteEvent());

  // `input_mappinging_widget_` is not created or destroyed in the view mode
  // when no active actions list, but it is created and shows up in `kEdit`
  // mode.
  RemoveAllActions();
  CheckWidgets(/*has_input_mapping_widget=*/true, /*hint_visible=*/true,
               /*has_editing_list_widget=*/true);
  EnableDisplayMode(DisplayMode::kView);
  // Don't create `input_mapping_widget_` if the action list is empty.
  CheckWidgets(/*has_input_mapping_widget=*/false, /*hint_visible=*/false,
               /*has_editing_list_widget=*/false);
  // `TouchInjector` stop rewriting events if there is no active action.
  EXPECT_FALSE(CanRewriteEvent());
}

}  // namespace arc::input_overlay
