// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"

#include <memory>

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "ui/aura/window.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace arc::input_overlay {

class EditingListTest : public OverlayViewTestBase {
 public:
  EditingListTest() = default;
  ~EditingListTest() override = default;

  size_t GetActionListItemsSize() {
    DCHECK(editing_list_->scroll_content_);
    DCHECK(editing_list_);
    if (editing_list_->HasControls()) {
      return editing_list_->scroll_content_->children().size();
    }
    return 0;
  }

  size_t GetActionViewSize() {
    DCHECK(input_mapping_view_);
    return input_mapping_view_->children().size();
  }

  size_t GetTouchInjectorActionSize() {
    DCHECK(touch_injector_);
    return touch_injector_->actions().size();
  }

  void PressAddButton() {
    DCHECK(editing_list_);
    editing_list_->OnAddButtonPressed();
  }

  void LeftClickAtActionViewListItem(int index) {
    if (!editing_list_ || index < 0) {
      return;
    }
    views::View* scroll_content = editing_list_->scroll_content_;
    DCHECK(scroll_content);
    if (index >= static_cast<int>(scroll_content->children().size())) {
      return;
    }

    auto* event_generator = GetEventGenerator();
    auto view_bounds = scroll_content->children()[index]->GetBoundsInScreen();
    // `ButtonOptionsMenu` may cover `EditingList`, so left-click on the left
    // side of the list item to avoid UI overlapping.
    event_generator->MoveMouseTo(view_bounds.x() + view_bounds.width() / 4,
                                 view_bounds.y() + view_bounds.height() / 2);
    event_generator->ClickLeftButton();
  }

  void MouseDragEditingListBy(int x, int y) {
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        static_cast<EditingList*>(GetEditingListWidget()->GetContentsView())
            ->editing_header_label_->GetBoundsInScreen()
            .CenterPoint());
    event_generator->PressLeftButton();
    event_generator->MoveMouseBy(x, y);
    event_generator->ReleaseLeftButton();
  }

  void TouchDragEditingListBy(int x, int y) {
    auto* event_generator = GetEventGenerator();

    event_generator->PressTouch(
        static_cast<EditingList*>(GetEditingListWidget()->GetContentsView())
            ->editing_header_label_->GetBoundsInScreen()
            .CenterPoint());
    event_generator->MoveTouchBy(x, y);
    event_generator->ReleaseTouch();
  }

  views::Widget* GetEditingListWidget() {
    return controller_->editing_list_widget_.get();
  }

  bool ButtonOptionsMenuExists() {
    return !!controller_->button_options_widget_;
  }

  Action* GetButtonOptionsAction() {
    return static_cast<ButtonOptionsMenu*>(
               controller_->button_options_widget_->GetContentsView())
        ->action();
  }
};

TEST_F(EditingListTest, TestAddNewAction) {
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  EXPECT_EQ(3u, GetActionListItemsSize());
  EXPECT_EQ(3u, GetActionViewSize());
  EXPECT_EQ(3u, GetTouchInjectorActionSize());
  EXPECT_FALSE(ButtonOptionsMenuExists());
  // Add a new action by pressing add button.
  PressAddButton();
  CheckActions(
      touch_injector_, /*expect_size=*/4u, /*expect_types=*/
      {ActionType::TAP, ActionType::TAP, ActionType::MOVE, ActionType::TAP},
      /*expect_ids=*/{0, 1, 2, kMaxDefaultActionID + 1});
  EXPECT_EQ(4u, GetActionListItemsSize());
  EXPECT_EQ(4u, GetActionViewSize());
  EXPECT_EQ(4u, GetTouchInjectorActionSize());
  EXPECT_TRUE(ButtonOptionsMenuExists());
}

TEST_F(EditingListTest, TestPressAtActionViewListItem) {
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  // Test action view list press.
  PressAddButton();
  EXPECT_TRUE(ButtonOptionsMenuExists());
  auto* action_1 = GetButtonOptionsAction();
  LeftClickAtActionViewListItem(/*index=*/0);
  EXPECT_TRUE(ButtonOptionsMenuExists());
  auto* action_2 = GetButtonOptionsAction();
  EXPECT_NE(action_1, action_2);
}

TEST_F(EditingListTest, TestReposition) {
  // 1. There is enough space on left and right of the sibling game window
  // outside.
  auto* editing_list_widget = GetEditingListWidget();
  auto initial_list_bounds = editing_list_widget->GetNativeWindow()->bounds();
  auto game_window_bounds = widget_->GetNativeWindow()->bounds();
  // It should be magnetic to the left side the sibling game window initially.
  EXPECT_EQ(gfx::Point(game_window_bounds.x(), initial_list_bounds.y()),
            initial_list_bounds.top_right());
  // Drag move `editing_list_widget` to the right. EditingList should be
  // magnetic to the right side of the sibling game window outside. No change on
  // y-axis.
  MouseDragEditingListBy(game_window_bounds.width(), 60);
  auto final_list_bounds = editing_list_widget->GetNativeWindow()->bounds();
  EXPECT_EQ(gfx::Point(game_window_bounds.right(), initial_list_bounds.y()),
            final_list_bounds.origin());

  // 2. Move game window position so there is no enough space on right outside.
  // It should be magnetic to the left side the sibling game window.
  widget_->GetNativeWindow()->SetBounds(gfx::Rect(600, 350, 300, 200));
  initial_list_bounds = editing_list_widget->GetNativeWindow()->bounds();
  game_window_bounds = widget_->GetNativeWindow()->bounds();
  EXPECT_EQ(gfx::Point(game_window_bounds.x(), initial_list_bounds.y()),
            initial_list_bounds.top_right());
  // Drag move `editing_list_widget` to the right. EditingList should be still
  // magnetic to the left side of the sibling game window outside.
  MouseDragEditingListBy(game_window_bounds.width(), 60);
  final_list_bounds = editing_list_widget->GetNativeWindow()->bounds();
  EXPECT_EQ(gfx::Point(game_window_bounds.x(), initial_list_bounds.y()),
            final_list_bounds.top_right());

  // 3. Set game window bounds so there is no enough space on both left and
  // right outside. `editing_list_widget` is magnetic to the left side and
  // inside of the sibling game window.
  widget_->GetNativeWindow()->SetBounds(gfx::Rect(100, 300, 800, 400));
  game_window_bounds = widget_->GetNativeWindow()->bounds();
  initial_list_bounds = editing_list_widget->GetNativeWindow()->bounds();
  const auto content_bounds =
      GetTouchInjector(widget_->GetNativeWindow())->content_bounds();
  auto content_bounds_origin = content_bounds.origin();
  content_bounds_origin.Offset(24, 24);
  EXPECT_EQ(content_bounds_origin, initial_list_bounds.origin());
  // Drag move `editing_list_widget` to the right. EditingList should be
  // magnetic to the right side of the sibling game window outside.
  TouchDragEditingListBy(game_window_bounds.width(), 30);
  final_list_bounds = editing_list_widget->GetNativeWindow()->bounds();
  auto content_bounds_top_right = content_bounds.top_right();
  content_bounds_top_right.Offset(-24, 24);
  EXPECT_EQ(content_bounds_top_right, final_list_bounds.top_right());
  // Drag move `editing_list_widget` to the left. EditingList should be
  // magnetic to the left side of the sibling game window outside.
  TouchDragEditingListBy(-game_window_bounds.width(), 40);
  final_list_bounds = editing_list_widget->GetNativeWindow()->bounds();
  EXPECT_EQ(content_bounds_origin, final_list_bounds.origin());
}

}  // namespace arc::input_overlay
