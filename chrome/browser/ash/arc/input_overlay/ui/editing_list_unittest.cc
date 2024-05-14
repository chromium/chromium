// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"

#include <memory>

#include "ash/style/icon_button.h"
#include "ash/system/toast/anchored_nudge.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "base/check.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_highlight.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/delete_edit_shortcut.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/nudge_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/target_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"
#include "components/ukm/test_ukm_recorder.h"
#include "ui/aura/window.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace arc::input_overlay {

class EditingListTest : public OverlayViewTestBase {
 public:
  EditingListTest() = default;
  ~EditingListTest() override = default;

  size_t GetTouchInjectorActionSize() {
    DCHECK(touch_injector_);
    return touch_injector_->actions().size();
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
    const auto view_bounds =
        scroll_content->children()[index]->GetBoundsInScreen();
    // `ButtonOptionsMenu` may cover `EditingList`, so left-click on the left
    // side of the list item to avoid UI overlapping.
    event_generator->MoveMouseTo(view_bounds.x() + view_bounds.width() / 4,
                                 view_bounds.y() + view_bounds.height() / 2);
    event_generator->ClickLeftButton();
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
    const auto* touch_point = action->action_view()->touch_point();
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
    if (!editing_list_ || !editing_list_->GetVisible()) {
      return;
    }
    views::View* scroll_content = editing_list_->scroll_content_;
    DCHECK(scroll_content);
    const int scroll_height = scroll_content->GetPreferredSize().height();
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

  // Verify action highlight is `expect_visible`. If it is visible, check if it
  // is anchored to `expect_anchor_view`.
  void VerifyActionHighlight(bool expect_visible,
                             ActionView* expect_anchor_view) {
    DCHECK(controller_);
    bool visible = false;
    ActionView* anchor_view = nullptr;
    if (auto* highlight_widget = controller_->action_highlight_widget_.get()) {
      visible = highlight_widget->IsVisible();
      auto* highlight = views::AsViewClass<ActionHighlight>(
          highlight_widget->GetContentsView());
      DCHECK(highlight);
      anchor_view = highlight->anchor_view();
    }

    EXPECT_EQ(expect_visible, visible);
    if (expect_visible) {
      EXPECT_EQ(expect_anchor_view, anchor_view);
    }
  }

  bool IsButtonOptionsMenuVisible() {
    const auto* menu_widget = controller_->button_options_widget_.get();
    return menu_widget && menu_widget->IsVisible();
  }

  bool IsEditingListVisible() {
    DCHECK(controller_);
    if (auto* editing_list_widget = controller_->editing_list_widget_.get()) {
      return editing_list_widget->IsVisible();
    }
    return false;
  }

  bool IsKeyEditNudgeShown() const {
    DCHECK(controller_);
    auto* editing_list = controller_->GetEditingList();
    DCHECK(editing_list);
    return editing_list->IsKeyEditNudgeShownForTesting();
  }

  ash::AnchoredNudge* GetKeyEditNudge() const {
    DCHECK(controller_);
    auto* editing_list = controller_->GetEditingList();
    DCHECK(editing_list);
    return editing_list->GetKeyEditNudgeForTesting();
  }
};

TEST_F(EditingListTest, TestAddNewAction) {
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  EXPECT_EQ(3u, GetActionListItemsSize());
  EXPECT_EQ(3u, GetActionViewSize());
  EXPECT_EQ(3u, GetTouchInjectorActionSize());
  EXPECT_FALSE(GetButtonOptionsMenu());
  // Press add button and it enters into the button placement mode.
  PressAddButton();
  auto* target_view = GetTargetView();
  EXPECT_TRUE(target_view);
  // Click on the `target_widget` and then the new action is added.
  LeftClickOn(target_view);
  EXPECT_FALSE(GetTargetView());
  CheckActions(
      touch_injector_, /*expect_size=*/4u, /*expect_types=*/
      {ActionType::TAP, ActionType::TAP, ActionType::MOVE, ActionType::TAP},
      /*expect_ids=*/{0, 1, 2, kMaxDefaultActionID + 1});
  EXPECT_EQ(4u, GetActionListItemsSize());
  EXPECT_EQ(4u, GetActionViewSize());
  EXPECT_EQ(4u, GetTouchInjectorActionSize());
  EXPECT_TRUE(GetButtonOptionsMenu());

  // Make sure `Action::touch_down_positions_` is not empty for the new action.
  auto* new_action = touch_injector_->actions()[3].get();
  EXPECT_FALSE(new_action->touch_down_positions().empty());
}

TEST_F(EditingListTest, TestVisibilityForButtonPlacementMode) {
  EXPECT_TRUE(IsEditingListVisible());
  // Enter into the button placement mode and press key `esc` to give up adding
  // a new action.
  PressAddButton();
  EXPECT_FALSE(IsEditingListVisible());
  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_TRUE(IsEditingListVisible());

  // Enter into the button placement mode and press key `enter` to add a new
  // action.
  PressAddButton();
  EXPECT_FALSE(IsEditingListVisible());
  event_generator->PressAndReleaseKey(ui::VKEY_RETURN, ui::EF_NONE);
  EXPECT_FALSE(IsEditingListVisible());
  PressDoneButtonOnButtonOptionsMenu();
  EXPECT_TRUE(IsEditingListVisible());

  // Enter into the button placement mode and press key `enter` to add a new
  // action.
  PressAddButton();
  EXPECT_FALSE(IsEditingListVisible());
  event_generator->PressAndReleaseKey(ui::VKEY_RETURN, ui::EF_NONE);
  EXPECT_FALSE(IsEditingListVisible());
  PressDeleteButtonOnButtonOptionsMenu();
  EXPECT_TRUE(IsEditingListVisible());

  // Shows any button options menu and the editing list is hidden.
  const auto& actions = touch_injector_->actions();
  DCHECK_GT(actions.size(), 1u);
  EXPECT_TRUE(ShowButtonOptionsMenu(actions[actions.size() - 1].get()));
  EXPECT_FALSE(IsEditingListVisible());
  PressDoneButtonOnButtonOptionsMenu();
  EXPECT_TRUE(IsEditingListVisible());
}

TEST_F(EditingListTest, TestDragAtNewAction) {
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  AddNewActionInCenter();
  EXPECT_TRUE(IsButtonOptionsMenuVisible());
  auto* action = GetButtonOptionsMenuAction();
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
  AddNewActionInCenter();
  EXPECT_TRUE(GetButtonOptionsMenu());
  auto* action_1 = GetButtonOptionsMenuAction();
  PressDoneButtonOnButtonOptionsMenu();
  // Scroll back to top to click the first list item.
  ScrollTo(/*top=*/true);
  LeftClickAtActionViewListItem(/*index=*/0);
  EXPECT_TRUE(GetButtonOptionsMenu());
  auto* action_2 = GetButtonOptionsMenuAction();
  EXPECT_NE(action_1, action_2);
  PressDoneButtonOnButtonOptionsMenu();
  LeftClickAtActionViewListItem(/*index=*/1);
  EXPECT_TRUE(GetButtonOptionsMenu());
  auto* action_3 = GetButtonOptionsMenuAction();
  EXPECT_NE(action_3, action_2);
}

TEST_F(EditingListTest, TestActionHighlight) {
  // 1. Hover without no view focused.
  VerifyActionHighlight(/*expect_visible=*/false,
                        /*expect_anchor_view=*/nullptr);

  HoverAtActionViewListItem(/*index=*/0u);
  VerifyActionHighlight(/*expect_visible=*/true,
                        /*expect_anchor_view=*/tap_action_->action_view());

  HoverAtActionViewListItem(/*index=*/1u);
  VerifyActionHighlight(/*expect_visible=*/true,
                        /*expect_anchor_view=*/tap_action_two_->action_view());

  // Hover outside of the list item and the view highlight is also removed.
  auto* list_item = GetEditingListItem(/*index=*/1u);
  DCHECK(list_item);
  auto item_origin = list_item->GetBoundsInScreen().origin();
  item_origin.Offset(-2, -2);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(item_origin);
  VerifyActionHighlight(/*expect_visible=*/false,
                        /*expect_anchor_view=*/nullptr);

  // 2. Hover with the focused list item.
  list_item->RequestFocus();
  EXPECT_EQ(list_item, list_item->GetFocusManager()->GetFocusedView());
  VerifyActionHighlight(/*expect_visible=*/true,
                        /*expect_anchor_view=*/tap_action_two_->action_view());
  HoverAtActionViewListItem(/*index=*/0u);
  VerifyActionHighlight(/*expect_visible=*/true,
                        /*expect_anchor_view=*/tap_action_->action_view());

  list_item->RequestFocus();
  EXPECT_EQ(list_item, list_item->GetFocusManager()->GetFocusedView());
  VerifyActionHighlight(/*expect_visible=*/true,
                        /*expect_anchor_view=*/tap_action_two_->action_view());
  HoverAtActionViewListItem(/*index=*/1u);
  VerifyActionHighlight(/*expect_visible=*/true,
                        /*expect_anchor_view=*/tap_action_two_->action_view());

  // 3. Hover with the focused edit label.
  // Clear high light first.
  event_generator->MoveMouseTo(item_origin);
  VerifyActionHighlight(/*expect_visible=*/false,
                        /*expect_anchor_view=*/nullptr);
  auto* edit_label = GetEditLabel(
      views::AsViewClass<ActionViewListItem>(list_item), /*index=*/0);
  edit_label->RequestFocus();
  VerifyActionHighlight(/*expect_visible=*/true,
                        /*expect_anchor_view=*/tap_action_two_->action_view());
  HoverAtActionViewListItem(/*index=*/0u);
  VerifyActionHighlight(/*expect_visible=*/true,
                        /*expect_anchor_view=*/tap_action_->action_view());

  edit_label->RequestFocus();
  VerifyActionHighlight(/*expect_visible=*/true,
                        /*expect_anchor_view=*/tap_action_two_->action_view());
  HoverAtActionViewListItem(/*index=*/1u);
  VerifyActionHighlight(/*expect_visible=*/true,
                        /*expect_anchor_view=*/tap_action_two_->action_view());

  // 4. Focus on the list item and then click on other view to move the
  // activation.
  list_item->RequestFocus();
  EXPECT_EQ(list_item, list_item->GetFocusManager()->GetFocusedView());
  VerifyActionHighlight(/*expect_visible=*/true,
                        /*expect_anchor_view=*/tap_action_two_->action_view());
  LeftClickOn(tap_action_->action_view());
  VerifyActionHighlight(/*expect_visible=*/false,
                        /*expect_anchor_view=*/nullptr);
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

TEST_F(EditingListTest, TestKeyEditNudge) {
  // Key edit nudge shows up after the first action's `ButtonOptionsMenu`.
  EXPECT_FALSE(IsKeyEditNudgeShown());

  // Remove all actions.
  for (const auto& action : touch_injector_->actions()) {
    controller_->RemoveAction(action.get());
  }

  // Add the first action.
  AddNewActionInCenter();
  PressDoneButtonOnButtonOptionsMenu();
  EXPECT_TRUE(IsKeyEditNudgeShown());

  // Click on key edit nudge and it still shows GC UIs.
  auto* key_edit_nudge = GetKeyEditNudge();
  EXPECT_TRUE(key_edit_nudge);
  LeftClickOn(key_edit_nudge->GetContentsView());
  EXPECT_TRUE(GetEditingListWidget());

  // Move mouse outside of the nudge and it will close in 10 seconds.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(0, 0));
  task_environment()->FastForwardBy(
      ash::AnchoredNudgeManagerImpl::kNudgeDefaultDuration);
  EXPECT_TRUE(IsKeyEditNudgeShown());
  task_environment()->FastForwardBy(
      ash::AnchoredNudgeManagerImpl::kNudgeMediumDuration);
  EXPECT_FALSE(IsKeyEditNudgeShown());

  // Open the button options menu and close it again, it won't show eidt nudge
  // again because it only shows once.
  const auto& actions = touch_injector_->actions();
  EXPECT_EQ(1u, GetActionListItemsSize());
  ShowButtonOptionsMenu(actions[actions.size() - 1].get());
  PressDoneButtonOnButtonOptionsMenu();
  EXPECT_FALSE(IsKeyEditNudgeShown());

  // No key edit nudge after adding another action.
  AddNewActionInCenter();
  PressDoneButtonOnButtonOptionsMenu();
  EXPECT_FALSE(IsKeyEditNudgeShown());
}

TEST_F(EditingListTest, TestScrollView) {
  widget_->GetNativeWindow()->SetBounds(gfx::Rect(310, 10, 300, 500));

  auto* list_window = GetEditingListWidget()->GetNativeWindow();
  int original_height = list_window->bounds().height();
  int window_content_height = touch_injector_->content_bounds().height();
  EXPECT_LE(list_window->bounds().height(), window_content_height);
  EXPECT_FALSE(GetScrollBarVisible());
  // Add new actions until it shows scroll bar.
  AddNewActionInCenter();
  PressDoneButtonOnButtonOptionsMenu();
  EXPECT_GT(list_window->bounds().height(), original_height);
  AddNewActionInCenter();
  PressDoneButtonOnButtonOptionsMenu();
  AddNewActionInCenter();
  PressDoneButtonOnButtonOptionsMenu();
  EXPECT_TRUE(GetScrollBarVisible());
  EXPECT_EQ(window_content_height, list_window->bounds().height());
  AddNewActionInCenter();
  PressDoneButtonOnButtonOptionsMenu();
  EXPECT_TRUE(GetScrollBarVisible());
  EXPECT_EQ(window_content_height, list_window->bounds().height());
  AddNewActionInCenter();
  PressDoneButtonOnButtonOptionsMenu();
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

TEST_F(EditingListTest, TestMaximumActions) {
  const size_t action_size = controller_->GetActiveActionsSize();
  // Add new action util it reaches to the maximum.
  EXPECT_GT(kMaxActionCount, action_size);
  for (size_t i = 0; i < kMaxActionCount - action_size; i++) {
    AddNewActionInCenter();
    PressDoneButtonOnButtonOptionsMenu();
  }

  // Once the actions size reaches to the maximum, press add buttons shouldn't
  // get into the button placement mode.
  PressAddButton();
  EXPECT_FALSE(GetTargetView());

  PressAddContainerButton();
  EXPECT_FALSE(GetTargetView());
}

TEST_F(EditingListTest, TestHistograms) {
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const std::string histogram_name =
      BuildGameControlsHistogramName(kEditingListFunctionTriggeredHistogram);
  std::map<EditingListFunction, int> expected_histogram_values;

  PressAddButton();
  MapIncreaseValueByOne(expected_histogram_values, EditingListFunction::kAdd);
  VerifyHistogramValues(histograms, histogram_name, expected_histogram_values);
  VerifyEditingListFunctionTriggeredUkmEvent(
      ukm_recorder, /*expected_entry_size=*/1u,
      static_cast<int64_t>(EditingListFunction::kAdd));

  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  HoverAtActionViewListItem(/*index=*/0u);
  MapIncreaseValueByOne(expected_histogram_values,
                        EditingListFunction::kHoverListItem);
  VerifyHistogramValues(histograms, histogram_name, expected_histogram_values);
  VerifyEditingListFunctionTriggeredUkmEvent(
      ukm_recorder, /*expected_entry_size=*/2u,
      static_cast<int64_t>(EditingListFunction::kHoverListItem));

  LeftClickAtActionViewListItem(/*index=*/0);
  MapIncreaseValueByOne(expected_histogram_values,
                        EditingListFunction::kPressListItem);
  VerifyHistogramValues(histograms, histogram_name, expected_histogram_values);
  VerifyEditingListFunctionTriggeredUkmEvent(
      ukm_recorder, /*expected_entry_size=*/3u,
      static_cast<int64_t>(EditingListFunction::kPressListItem));
}

}  // namespace arc::input_overlay
