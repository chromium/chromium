// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"

#include <memory>

#include "ash/style/icon_button.h"
#include "base/check.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/delete_edit_shortcut.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"
#include "ui/aura/window.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
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

  size_t GetTouchInjectorActionSize() {
    DCHECK(touch_injector_);
    return touch_injector_->actions().size();
  }

  // Add a new action in the center of the main window.
  void AddNewAction() {
    PressAddButton();
    auto* target_widget = GetTargetViewWidget();
    DCHECK(target_widget);
    LeftClickOn(target_widget->GetContentsView());
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

  void HoverAtActionViewListItem(int index) {
    if (!editing_list_ || editing_list_->is_zero_state_ || index < 0) {
      return;
    }
    views::View* scroll_content = editing_list_->scroll_content_;
    DCHECK(scroll_content);
    if (index >= static_cast<int>(scroll_content->children().size())) {
      return;
    }

    auto* event_generator = GetEventGenerator();
    auto view_bounds = scroll_content->children()[index]->GetBoundsInScreen();
    event_generator->MoveMouseTo(view_bounds.CenterPoint());
  }

  void MouseDragEditingListBy(int x, int y) {
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        editing_list_->editing_header_label_->GetBoundsInScreen()
            .CenterPoint());
    event_generator->PressLeftButton();
    event_generator->MoveMouseBy(x, y);
    event_generator->ReleaseLeftButton();
  }

  // Mouse dragging `action` view by `(x, y)` without releasing mouse left
  // button.
  void MouseDraggingActionViewBy(Action* action, int x, int y) {
    auto* event_generator = GetEventGenerator();
    auto* touch_point = action->action_view()->touch_point();
    if (!touch_point) {
      LOG(WARNING) << "Mouse dragging has no valid touch point.";
      return;
    }
    event_generator->MoveMouseTo(
        touch_point->GetBoundsInScreen().CenterPoint());
    event_generator->PressLeftButton();
    event_generator->MoveMouseBy(x, y);
  }

  void TouchDragEditingListBy(int x, int y) {
    auto* event_generator = GetEventGenerator();

    event_generator->PressTouch(
        editing_list_->editing_header_label_->GetBoundsInScreen()
            .CenterPoint());
    event_generator->MoveTouchBy(x, y);
    event_generator->ReleaseTouch();
  }

  void ScrollTo(bool top) {
    if (!editing_list_) {
      return;
    }
    views::View* scroll_content = editing_list_->scroll_content_;
    DCHECK(scroll_content);
    int scroll_height = scroll_content->GetPreferredSize().height();
    editing_list_->scroll_view_->ScrollByOffset(
        gfx::PointF(0, top ? -scroll_height : scroll_height));
  }

  bool GetScrollBarVisible() {
    return editing_list_->scroll_view_->vertical_scroll_bar()->GetVisible();
  }

  views::Widget* GetEditingListWidget() {
    return controller_->editing_list_widget_.get();
  }

  views::Widget* GetInputMappingWidget() {
    return controller_->input_mapping_widget_.get();
  }

  bool ButtonOptionsMenuExists() {
    return !!controller_->button_options_widget_;
  }

  bool DeleteEditShortcutExists() {
    return !!controller_->delete_edit_shortcut_widget_;
  }

  bool IsActionHighlightVisible() {
    if (!controller_->action_highlight_widget_) {
      return false;
    }
    return controller_->action_highlight_widget_->GetContentsView()
        ->GetVisible();
  }

  bool IsButtonOptionsMenuVisible() {
    auto* menu_widget = controller_->button_options_widget_.get();
    return menu_widget && menu_widget->IsVisible();
  }

  void PressDoneButtonOnButtonOptionsMenu() {
    auto* menu = controller_->GetButtonOptionsMenu();
    if (menu) {
      LeftClickOn(menu->done_button_);
    }
  }

  void PressEditButton() {
    DCHECK(controller_->delete_edit_shortcut_widget_);
    static_cast<DeleteEditShortcut*>(
        controller_->delete_edit_shortcut_widget_->GetContentsView())
        ->OnEditButtonPressed();
  }

  void PressDeleteButton() {
    DCHECK(controller_->delete_edit_shortcut_widget_);
    static_cast<DeleteEditShortcut*>(
        controller_->delete_edit_shortcut_widget_->GetContentsView())
        ->OnDeleteButtonPressed();
  }

  Action* GetButtonOptionsAction() {
    auto* menu = controller_->GetButtonOptionsMenu();
    if (!menu) {
      return nullptr;
    }
    return menu->action();
  }

  Action* GetDeleteEditShortcutAction() {
    return static_cast<DeleteEditShortcut*>(
               controller_->delete_edit_shortcut_widget_->GetContentsView())
        ->anchor_view()
        ->action();
  }

  views::Widget* GetEducationNudge(views::Widget* widget) {
    DCHECK(controller_);
    auto& widgets_map = controller_->nudge_widgets_;
    return widgets_map.contains(widget) ? widgets_map.find(widget)->second.get()
                                        : nullptr;
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
  // Press add button and it enters into the button placement mode.
  PressAddButton();
  auto* target_widget = GetTargetViewWidget();
  EXPECT_TRUE(target_widget);
  // Click on the `target_widget` and then the new action is added.
  LeftClickOn(target_widget->GetContentsView());
  EXPECT_FALSE(GetTargetViewWidget());
  CheckActions(
      touch_injector_, /*expect_size=*/4u, /*expect_types=*/
      {ActionType::TAP, ActionType::TAP, ActionType::MOVE, ActionType::TAP},
      /*expect_ids=*/{0, 1, 2, kMaxDefaultActionID + 1});
  EXPECT_EQ(4u, GetActionListItemsSize());
  EXPECT_EQ(4u, GetActionViewSize());
  EXPECT_EQ(4u, GetTouchInjectorActionSize());
  EXPECT_TRUE(ButtonOptionsMenuExists());

  // Make sure `Action::touch_down_positions_` is not empty for the new action.
  auto* new_action = touch_injector_->actions()[3].get();
  EXPECT_FALSE(new_action->touch_down_positions().empty());
}

TEST_F(EditingListTest, TestDragAtNewAction) {
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  AddNewAction();
  EXPECT_TRUE(IsButtonOptionsMenuVisible());
  auto* action = GetButtonOptionsAction();
  EXPECT_TRUE(action->is_new());
  MouseDraggingActionViewBy(action, /*x=*/10, /*y=*/10);
  EXPECT_FALSE(IsButtonOptionsMenuVisible());
  EXPECT_TRUE(action->is_new());
  GetEventGenerator()->ReleaseLeftButton();
  EXPECT_TRUE(IsButtonOptionsMenuVisible());
  EXPECT_TRUE(action->is_new());
}

TEST_F(EditingListTest, TestPressAtActionViewListItem) {
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  // Test action view list press.
  AddNewAction();
  EXPECT_TRUE(ButtonOptionsMenuExists());
  auto* action_1 = GetButtonOptionsAction();
  // Scroll back to top to click the first list item.
  ScrollTo(/*top=*/true);
  LeftClickAtActionViewListItem(/*index=*/0);
  EXPECT_TRUE(ButtonOptionsMenuExists());
  auto* action_2 = GetButtonOptionsAction();
  EXPECT_NE(action_1, action_2);
}

TEST_F(EditingListTest, TestHoverAtActionViewListItem) {
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  HoverAtActionViewListItem(/*index=*/0);
  EXPECT_TRUE(DeleteEditShortcutExists());
  EXPECT_TRUE(IsActionHighlightVisible());
  auto* action = GetDeleteEditShortcutAction();

  PressEditButton();
  EXPECT_TRUE(ButtonOptionsMenuExists());
  EXPECT_FALSE(DeleteEditShortcutExists());
  EXPECT_EQ(action, GetButtonOptionsAction());
  PressDoneButtonOnButtonOptionsMenu();

  HoverAtActionViewListItem(/*index=*/0);
  EXPECT_TRUE(DeleteEditShortcutExists());
  EXPECT_TRUE(IsActionHighlightVisible());
  PressDeleteButton();
  EXPECT_FALSE(DeleteEditShortcutExists());
  EXPECT_EQ(2u, GetActionListItemsSize());
  EXPECT_EQ(2u, GetActionViewSize());
}

TEST_F(EditingListTest, TestReposition) {
  // 1. There is enough space on left and right of the sibling game window
  // outside.
  auto* editing_list_widget = GetEditingListWidget();
  auto initial_list_bounds = editing_list_widget->GetNativeWindow()->bounds();
  auto game_window_bounds = widget_->GetNativeWindow()->bounds();
  // It should be magnetic to the left side the sibling game window initially.
  EXPECT_EQ(
      gfx::Point(game_window_bounds.x() - kEditingListSpaceBetweenMainWindow,
                 initial_list_bounds.y()),
      initial_list_bounds.top_right());
  // Drag move `editing_list_widget` to the right. EditingList should be
  // magnetic to the right side of the sibling game window outside. No change on
  // y-axis.
  MouseDragEditingListBy(game_window_bounds.width() / 2 +
                             initial_list_bounds.width() / 2 +
                             kEditingListSpaceBetweenMainWindow,
                         60);
  auto final_list_bounds = editing_list_widget->GetNativeWindow()->bounds();
  EXPECT_EQ(gfx::Point(
                game_window_bounds.right() + kEditingListSpaceBetweenMainWindow,
                initial_list_bounds.y()),
            final_list_bounds.origin());

  // 2. Move game window position so there is no enough space on right outside.
  // It should be magnetic to the left side the sibling game window.
  widget_->GetNativeWindow()->SetBounds(gfx::Rect(600, 350, 300, 200));
  initial_list_bounds = editing_list_widget->GetNativeWindow()->bounds();
  game_window_bounds = widget_->GetNativeWindow()->bounds();
  EXPECT_EQ(
      gfx::Point(game_window_bounds.x() - kEditingListSpaceBetweenMainWindow,
                 initial_list_bounds.y()),
      initial_list_bounds.top_right());
  // Drag move `editing_list_widget` to the right. EditingList should be still
  // magnetic to the left side of the sibling game window outside.
  MouseDragEditingListBy(game_window_bounds.width() / 2 +
                             initial_list_bounds.width() / 2 +
                             kEditingListSpaceBetweenMainWindow,
                         60);
  final_list_bounds = editing_list_widget->GetNativeWindow()->bounds();
  EXPECT_EQ(
      gfx::Point(game_window_bounds.x() - kEditingListSpaceBetweenMainWindow,
                 initial_list_bounds.y()),
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
  TouchDragEditingListBy(game_window_bounds.width() / 2 +
                             initial_list_bounds.width() / 2 +
                             kEditingListSpaceBetweenMainWindow,
                         30);
  final_list_bounds = editing_list_widget->GetNativeWindow()->bounds();
  auto content_bounds_top_right = content_bounds.top_right();
  content_bounds_top_right.Offset(-24, 24);
  EXPECT_EQ(content_bounds_top_right, final_list_bounds.top_right());
  // Drag move `editing_list_widget` to the left. EditingList should be
  // magnetic to the left side of the sibling game window outside.
  TouchDragEditingListBy(
      -(game_window_bounds.width() / 2 + initial_list_bounds.width() / 2 +
        kEditingListSpaceBetweenMainWindow),
      40);
  final_list_bounds = editing_list_widget->GetNativeWindow()->bounds();
  EXPECT_EQ(content_bounds_origin, final_list_bounds.origin());
}

TEST_F(EditingListTest, TestEducationNudge) {
  // 1. Education nudge shows up for `EditingList` by "+" button when there is
  // no active action in the list.
  // 2. After adding the first action, the education nudge shows up for input
  // mapping by the touch point.
  // 3. After closing `ButtonOptionsMenu`, the education nudge shows up for
  // `EditingList`.
  auto* editing_list = GetEditingListWidget();
  auto* input_mapping = GetInputMappingWidget();
  DCHECK(editing_list);
  DCHECK(input_mapping);
  EXPECT_FALSE(GetEducationNudge(editing_list));
  EXPECT_FALSE(GetEducationNudge(input_mapping));
  // Remove all actions.
  for (const auto& action : touch_injector_->actions()) {
    controller_->RemoveAction(action.get());
  }
  // The education nudge for `editing_list` shows up.
  auto* editing_list_nudge = GetEducationNudge(editing_list);
  EXPECT_TRUE(editing_list);
  EXPECT_TRUE(
      editing_list_nudge->IsStackedAbove(editing_list->GetNativeView()));
  EXPECT_FALSE(GetEducationNudge(input_mapping));

  // Add an action, the previous education nudge for `editing_list` is removed.
  AddNewAction();
  EXPECT_FALSE(GetEducationNudge(editing_list));
  // The education nudge for `input_mapping` shows up.
  auto* input_mapping_nudge = GetEducationNudge(input_mapping);
  EXPECT_TRUE(input_mapping_nudge);
  EXPECT_TRUE(
      input_mapping_nudge->IsStackedAbove(input_mapping->GetNativeView()));

  // After editing the first action, which means closing `ButtonOptionsMenu`,
  // education nudge shows up for editing tip.
  PressDoneButtonOnButtonOptionsMenu();
  editing_list_nudge = GetEducationNudge(editing_list);
  EXPECT_TRUE(editing_list_nudge);
  EXPECT_TRUE(
      editing_list_nudge->IsStackedAbove(editing_list->GetNativeView()));
  EXPECT_FALSE(GetEducationNudge(input_mapping));

  // No education nudge after adding another action.
  AddNewAction();
  PressDoneButtonOnButtonOptionsMenu();
  EXPECT_FALSE(GetEducationNudge(editing_list));
  EXPECT_FALSE(GetEducationNudge(input_mapping));
}

TEST_F(EditingListTest, TestScrollView) {
  widget_->GetNativeWindow()->SetBounds(gfx::Rect(310, 10, 300, 500));

  auto* list_window = GetEditingListWidget()->GetNativeWindow();
  int original_height = list_window->bounds().height();
  int window_content_height = touch_injector_->content_bounds().height();
  EXPECT_LE(list_window->bounds().height(), window_content_height);
  EXPECT_FALSE(GetScrollBarVisible());
  // Add new actions until it shows scroll bar.
  AddNewAction();
  EXPECT_GT(list_window->bounds().height(), original_height);
  AddNewAction();
  AddNewAction();
  EXPECT_TRUE(GetScrollBarVisible());
  EXPECT_EQ(window_content_height, list_window->bounds().height());
  AddNewAction();
  EXPECT_TRUE(GetScrollBarVisible());
  EXPECT_EQ(window_content_height, list_window->bounds().height());
  AddNewAction();
  EXPECT_TRUE(GetScrollBarVisible());
  EXPECT_EQ(window_content_height, list_window->bounds().height());

  // Add the game window height by 50, EditingList height is also added by 50.
  widget_->GetNativeWindow()->SetBounds(gfx::Rect(310, 20, 300, 550));
  EXPECT_EQ(window_content_height + 50,
            touch_injector_->content_bounds().height());
  EXPECT_EQ(window_content_height + 50, list_window->bounds().height());

  // Make the game window bounds larger so EditingList will be placed inside.
  widget_->GetNativeWindow()->SetBounds(gfx::Rect(100, 300, 800, 400));
  window_content_height = touch_injector_->content_bounds().height();
  EXPECT_EQ(window_content_height, list_window->bounds().height() +
                                       kEditingListOffsetInsideMainWindow);
}

}  // namespace arc::input_overlay
