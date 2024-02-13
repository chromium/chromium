// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"

#include "ash/public/cpp/arc_game_controls_flag.h"
#include "chrome/browser/ash/arc/input_overlay/test/game_controls_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/delete_edit_shortcut.h"
#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "ui/aura/window.h"
#include "ui/events/event_constants.h"
#include "ui/views/widget/widget.h"

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

  void EnableGIO(aura::Window* window, bool enable) {
    // In GD, once Game Controls feature is turned on or off, the
    // mapping hint is also set to on or off.
    window->SetProperty(
        ash::kArcGameControlsFlagsKey,
        UpdateFlag(window->GetProperty(ash::kArcGameControlsFlagsKey),
                   static_cast<ash::ArcGameControlsFlag>(
                       ash::ArcGameControlsFlag::kEnabled |
                       ash::ArcGameControlsFlag::kHint),
                   /*enable_flag=*/enable));
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

  // 1. Disable and enable GIO in `kView` mode. All the widgets shouldn't show
  // up when GIO is disabled.
  // Disable GIO. In GD, once Game Controls feature is turned on or off, the
  // mapping hint is also set to on or off.
  EnableGIO(window, /*enable=*/false);
  CheckWidgets(/*has_input_mapping_widget=*/false,
               /*hint_visible=*/false, /*has_editing_list_widget=*/false);
  EXPECT_FALSE(CanRewriteEvent());
  // Enable GIO back.
  EnableGIO(window, /*enable=*/true);
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
  EnableGIO(window, /*enable=*/false);
  CheckWidgets(/*has_input_mapping_widget=*/false, /*hint_visible=*/false,
               /*has_editing_list_widget=*/false);
  // Enable GIO and overlay is displayed as view mode.
  EnableGIO(window, /*enable=*/true);
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
      UpdateFlag(window->GetProperty(ash::kArcGameControlsFlagsKey),
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

// -----------------------------------------------------------------------------
// EditModeDisplayOverlayControllerTest:
// For test in the edit mode.
class EditModeDisplayOverlayControllerTest : public OverlayViewTestBase {
 public:
  EditModeDisplayOverlayControllerTest() = default;
  ~EditModeDisplayOverlayControllerTest() override = default;

  void CheckWidgetsVisible(bool input_mapping_visible,
                           bool editing_list_visible,
                           bool button_options_visible,
                           bool delete_edit_menu_visible) {
    EXPECT_EQ(
        input_mapping_visible,
        input_mapping_view_ && input_mapping_view_->GetWidget()->IsVisible());

    auto* editing_list = GetEditingList();
    EXPECT_EQ(editing_list_visible,
              editing_list && editing_list->GetWidget()->IsVisible());

    auto* button_options_menu = GetButtonOptionsMenu();
    EXPECT_EQ(
        button_options_visible,
        button_options_menu && button_options_menu->GetWidget()->IsVisible());

    auto* delete_edit_view = GetDeleteEditShortcut();
    EXPECT_EQ(delete_edit_menu_visible,
              delete_edit_view && delete_edit_view->GetWidget()->IsVisible());
  }

  // Presses key tab until it focuses on the first focusable view in
  // `contents_view` when `reverse` is true, or the last focusable view on
  // `contents_view` when `reverse` is false.
  void PressTabKeyToFirstOrLastElement(views::View* contents_view,
                                       bool reverse) {
    auto* event_generator = GetEventGenerator();
    auto* focus_manager = contents_view->GetFocusManager();

    while (true) {
      auto* next_focus = focus_manager->GetNextFocusableView(
          /*starting_view=*/focus_manager->GetFocusedView(),
          /*starting_widget=*/contents_view->GetWidget(), reverse,
          /*dont_loop=*/true);
      if (!next_focus) {
        break;
      }
      event_generator->PressAndReleaseKey(
          ui::KeyboardCode::VKEY_TAB,
          (reverse ? ui::EF_SHIFT_DOWN : ui::EF_NONE));
      EXPECT_TRUE(focus_manager->GetFocusedView());
    }
  }
};

TEST_F(EditModeDisplayOverlayControllerTest, TestFocusCycler) {
  CheckWidgetsVisible(
      /*input_mapping_visible=*/true, /*editing_list_visible=*/true,
      /*button_options_visible=*/false, /*delete_edit_menu_visible=*/false);

  // Case 1: in edit mode default view. The tab focus will cycle between the
  // editing list and input mapping. Press key tab to the last element of the
  // editing list.
  auto* editing_list = GetEditingList();
  PressTabKeyToFirstOrLastElement(editing_list, /*reverse=*/false);
  auto* list_focus_manager = editing_list->GetFocusManager();
  EXPECT_TRUE(list_focus_manager->GetFocusedView());

  // Press key tab to focus on the input mapping.
  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  EXPECT_FALSE(list_focus_manager->GetFocusedView());
  auto* mapping_focus_manager = input_mapping_view_->GetFocusManager();
  EXPECT_TRUE(mapping_focus_manager->GetFocusedView());

  // Keep pressing key tap to the last element of the input mapping.
  PressTabKeyToFirstOrLastElement(input_mapping_view_, /*reverse=*/false);
  EXPECT_TRUE(mapping_focus_manager->GetFocusedView());
  EXPECT_FALSE(list_focus_manager->GetFocusedView());

  // Press key tab to focus on the editing list.
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  EXPECT_FALSE(mapping_focus_manager->GetFocusedView());
  EXPECT_TRUE(list_focus_manager->GetFocusedView());

  // Press tab + shift and it focuses back to the input mapping.
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB,
                                      ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(mapping_focus_manager->GetFocusedView());
  EXPECT_FALSE(list_focus_manager->GetFocusedView());

  // Case 2: show button options menu. The tab focus cycles between the
  // button options menu and input mapping. (editing list is hidden when button
  // options menu shows up.)
  ShowButtonOptionsMenu(tap_action_);
  CheckWidgetsVisible(
      /*input_mapping_visible=*/true, /*editing_list_visible=*/false,
      /*button_options_visible=*/true, /*delete_edit_menu_visible=*/false);
  auto* button_options_menu = GetButtonOptionsMenu();
  auto* options_focus_manager = button_options_menu->GetFocusManager();
  EXPECT_FALSE(mapping_focus_manager->GetFocusedView());
  EXPECT_FALSE(list_focus_manager->GetFocusedView());
  EXPECT_FALSE(options_focus_manager->GetFocusedView());

  // Keep pressing key tap to the last element of the button options menu.
  PressTabKeyToFirstOrLastElement(button_options_menu, /*reverse=*/false);
  EXPECT_FALSE(mapping_focus_manager->GetFocusedView());
  EXPECT_TRUE(options_focus_manager->GetFocusedView());

  // Press key tab to focus on the input mapping.
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  EXPECT_TRUE(mapping_focus_manager->GetFocusedView());
  EXPECT_FALSE(options_focus_manager->GetFocusedView());

  // Close button options menu and editing list shows back.
  PressDeleteButtonOnButtonOptionsMenu();
  CheckWidgetsVisible(
      /*input_mapping_visible=*/true, /*editing_list_visible=*/true,
      /*button_options_visible=*/false, /*delete_edit_menu_visible=*/false);

  // Case 3: show delete-edit menu. The tab focus cycles among the delete-edit
  // menu, editing list and input mapping.
  HoverAtActionViewListItem(/*index=*/1u);
  CheckWidgetsVisible(
      /*input_mapping_visible=*/true, /*editing_list_visible=*/true,
      /*button_options_visible=*/false, /*delete_edit_menu_visible=*/true);
  auto* delete_edit_shortcut = GetDeleteEditShortcut();
  auto* delete_edit_focus_manager = delete_edit_shortcut->GetFocusManager();
  EXPECT_FALSE(mapping_focus_manager->GetFocusedView());
  EXPECT_FALSE(list_focus_manager->GetFocusedView());
  EXPECT_FALSE(delete_edit_focus_manager->GetFocusedView());

  PressTabKeyToFirstOrLastElement(delete_edit_shortcut, /*reverse=*/false);
  EXPECT_FALSE(mapping_focus_manager->GetFocusedView());
  EXPECT_FALSE(list_focus_manager->GetFocusedView());
  EXPECT_TRUE(delete_edit_focus_manager->GetFocusedView());
  // Press key tab to focus on the input mapping.
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  EXPECT_TRUE(mapping_focus_manager->GetFocusedView());
  EXPECT_FALSE(list_focus_manager->GetFocusedView());
  EXPECT_FALSE(delete_edit_focus_manager->GetFocusedView());
}

}  // namespace arc::input_overlay
