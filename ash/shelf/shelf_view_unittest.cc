// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/overflow_bubble.h"
#include "ash/shelf/overflow_bubble_view.h"
#include "ash/shelf/overflow_bubble_view_test_api.h"
#include "ash/shelf/overflow_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/system/flag_warning/flag_warning_tray.h"
#include "ash/system/message_center/notification_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace ash {
namespace {

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

class TestShelfObserver : public ShelfObserver {
 public:
  explicit TestShelfObserver(Shelf* shelf) : shelf_(shelf) {
    shelf_->AddObserver(this);
  }

  ~TestShelfObserver() override { shelf_->RemoveObserver(this); }

  // ShelfObserver implementation.
  void OnShelfIconPositionsChanged() override {
    icon_positions_changed_ = true;

    icon_positions_animation_duration_ =
        ShelfViewTestAPI(shelf_->GetShelfViewForTesting())
            .GetAnimationDuration();
  }

  bool icon_positions_changed() const { return icon_positions_changed_; }
  void Reset() {
    icon_positions_changed_ = false;
    icon_positions_animation_duration_ = 0;
  }
  int icon_positions_animation_duration() const {
    return icon_positions_animation_duration_;
  }

 private:
  Shelf* shelf_;
  bool icon_positions_changed_ = false;
  int icon_positions_animation_duration_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestShelfObserver);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ShelfObserver::OnShelfIconPositionsChanged tests.

class ShelfObserverIconTest : public AshTestBase {
 public:
  ShelfObserverIconTest() = default;
  ~ShelfObserverIconTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    observer_.reset(new TestShelfObserver(GetPrimaryShelf()));
    shelf_view_test_.reset(
        new ShelfViewTestAPI(GetPrimaryShelf()->GetShelfViewForTesting()));
    shelf_view_test_->SetAnimationDuration(1);
  }

  void TearDown() override {
    observer_.reset();
    AshTestBase::TearDown();
  }

  TestShelfObserver* observer() { return observer_.get(); }

  ShelfViewTestAPI* shelf_view_test() { return shelf_view_test_.get(); }

 private:
  std::unique_ptr<TestShelfObserver> observer_;
  std::unique_ptr<ShelfViewTestAPI> shelf_view_test_;

  DISALLOW_COPY_AND_ASSIGN(ShelfObserverIconTest);
};

// A ShelfItemDelegate that tracks selections and reports a custom action.
class ShelfItemSelectionTracker : public ShelfItemDelegate {
 public:
  ShelfItemSelectionTracker() : ShelfItemDelegate(ShelfID()) {}
  ~ShelfItemSelectionTracker() override = default;

  size_t item_selected_count() const { return item_selected_count_; }
  void set_item_selected_action(ShelfAction item_selected_action) {
    item_selected_action_ = item_selected_action;
  }

  // ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback) override {
    item_selected_count_++;
    std::move(callback).Run(item_selected_action_, base::nullopt);
  }
  void ExecuteCommand(bool, int64_t, int32_t, int64_t) override {}
  void Close() override {}

 private:
  size_t item_selected_count_ = 0;
  ShelfAction item_selected_action_ = SHELF_ACTION_NONE;

  DISALLOW_COPY_AND_ASSIGN(ShelfItemSelectionTracker);
};

TEST_F(ShelfObserverIconTest, AddRemove) {
  ShelfItem item;
  item.id = ShelfID("foo");
  item.type = TYPE_APP;
  EXPECT_FALSE(observer()->icon_positions_changed());
  const int shelf_item_index = Shell::Get()->shelf_model()->Add(item);
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->icon_positions_changed());
  observer()->Reset();

  EXPECT_FALSE(observer()->icon_positions_changed());
  Shell::Get()->shelf_model()->RemoveItemAt(shelf_item_index);
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->icon_positions_changed());
  observer()->Reset();
}

// Make sure creating/deleting an window on one displays notifies a
// shelf on external display as well as one on primary.
TEST_F(ShelfObserverIconTest, AddRemoveWithMultipleDisplays) {
  UpdateDisplay("400x400,400x400");
  observer()->Reset();

  Shelf* second_shelf = Shelf::ForWindow(Shell::GetAllRootWindows()[1]);
  TestShelfObserver second_observer(second_shelf);

  ShelfItem item;
  item.id = ShelfID("foo");
  item.type = TYPE_APP;
  EXPECT_FALSE(observer()->icon_positions_changed());
  EXPECT_FALSE(second_observer.icon_positions_changed());
  const int shelf_item_index = Shell::Get()->shelf_model()->Add(item);
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->icon_positions_changed());
  EXPECT_TRUE(second_observer.icon_positions_changed());
  observer()->Reset();
  second_observer.Reset();

  EXPECT_FALSE(observer()->icon_positions_changed());
  EXPECT_FALSE(second_observer.icon_positions_changed());
  Shell::Get()->shelf_model()->RemoveItemAt(shelf_item_index);
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->icon_positions_changed());
  EXPECT_TRUE(second_observer.icon_positions_changed());

  observer()->Reset();
  second_observer.Reset();
}

TEST_F(ShelfObserverIconTest, BoundsChanged) {
  views::Widget* widget =
      GetPrimaryShelf()->GetShelfViewForTesting()->GetWidget();
  gfx::Rect shelf_bounds = widget->GetWindowBoundsInScreen();
  shelf_bounds.set_width(shelf_bounds.width() / 2);
  ASSERT_GT(shelf_bounds.width(), 0);
  widget->SetBounds(shelf_bounds);
  // No animation happens for ShelfView bounds change.
  EXPECT_TRUE(observer()->icon_positions_changed());
  observer()->Reset();
}

////////////////////////////////////////////////////////////////////////////////
// ShelfView tests.

class ShelfViewTest : public AshTestBase {
 public:
  static const char*
      kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName;

  ShelfViewTest() = default;
  ~ShelfViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    model_ = Shell::Get()->shelf_model();
    shelf_view_ = GetPrimaryShelf()->GetShelfViewForTesting();

    NotificationTray::DisableAnimationsForTest(true);

    // Several tests in this file are brittle and fail when the shelf width
    // changes due to the flag warning button. Since we won't show this button
    // to users in production, hide it in the tests. https://crbug.com/891080
    // TODO(jamescook): Remove this when SingleProcessMash is on by default or
    // when we remove the warning button.
    if (::features::IsSingleProcessMash()) {
      FlagWarningTray* flag_warning = GetPrimaryShelf()
                                          ->GetStatusAreaWidget()
                                          ->flag_warning_tray_for_testing();
      ASSERT_TRUE(flag_warning);
      flag_warning->SetVisible(false);
    }

    // The bounds should be big enough for 4 buttons + overflow chevron.
    ASSERT_GE(shelf_view_->width(), 500);

    test_api_.reset(new ShelfViewTestAPI(shelf_view_));
    test_api_->SetAnimationDuration(1);  // Speeds up animation for test.

    // Add a browser shortcut shelf item, as chrome does, for testing.
    AddItem(TYPE_BROWSER_SHORTCUT, true);
  }

  void TearDown() override {
    NotificationTray::DisableAnimationsForTest(false);  // Reenable animation
    test_api_.reset();
    AshTestBase::TearDown();
  }

  std::string GetNextAppId() { return base::IntToString(id_); }

 protected:
  // Add shelf items of various types, and optionally wait for animations.
  ShelfID AddItem(ShelfItemType type, bool wait_for_animations) {
    ShelfItem item;
    item.type = type;
    if (type == TYPE_APP)
      item.status = STATUS_RUNNING;

    item.id = ShelfID(base::IntToString(id_++));
    model_->Add(item);
    // Set a delegate; some tests require one to select the item.
    model_->SetShelfItemDelegate(item.id,
                                 std::make_unique<ShelfItemSelectionTracker>());
    if (wait_for_animations)
      test_api_->RunMessageLoopUntilAnimationsDone();
    return item.id;
  }
  ShelfID AddAppShortcut() { return AddItem(TYPE_PINNED_APP, true); }
  ShelfID AddAppNoWait() { return AddItem(TYPE_APP, false); }
  ShelfID AddApp() { return AddItem(TYPE_APP, true); }

  void SetShelfItemTypeToAppShortcut(const ShelfID& id) {
    int index = model_->ItemIndexByID(id);
    DCHECK_GE(index, 0);

    ShelfItem item = model_->items()[index];

    if (item.type == TYPE_APP) {
      item.type = TYPE_PINNED_APP;
      model_->Set(index, item);
    }
    test_api_->RunMessageLoopUntilAnimationsDone();
  }

  void RemoveByID(const ShelfID& id) {
    model_->RemoveItemAt(model_->ItemIndexByID(id));
    test_api_->RunMessageLoopUntilAnimationsDone();
  }

  ShelfButton* GetButtonByID(const ShelfID& id) {
    return test_api_->GetButton(model_->ItemIndexByID(id));
  }

  ShelfItem GetItemByID(const ShelfID& id) { return *model_->ItemByID(id); }

  void PinAppWithID(const ShelfID& id) { model_->PinAppWithID(id.app_id); }

  bool IsAppPinned(const ShelfID& id) { return model_->IsAppPinned(id.app_id); }

  void CheckModelIDs(
      const std::vector<std::pair<ShelfID, views::View*>>& id_map) {
    size_t map_index = 0;
    for (size_t model_index = 0; model_index < model_->items().size();
         ++model_index) {
      ShelfItem item = model_->items()[model_index];
      ShelfID id = item.id;
      EXPECT_EQ(id_map[map_index].first, id);
      EXPECT_EQ(id_map[map_index].second, GetButtonByID(id));
      ++map_index;
    }
    ASSERT_EQ(map_index, id_map.size());
  }

  void VerifyShelfItemBoundsAreValid() {
    for (int i = 0; i <= test_api_->GetLastVisibleIndex(); ++i) {
      if (test_api_->GetButton(i)) {
        gfx::Rect shelf_view_bounds = shelf_view_->GetLocalBounds();
        gfx::Rect item_bounds = test_api_->GetBoundsByIndex(i);
        EXPECT_GE(item_bounds.x(), 0);
        EXPECT_GE(item_bounds.y(), 0);
        EXPECT_LE(item_bounds.right(), shelf_view_bounds.width());
        EXPECT_LE(item_bounds.bottom(), shelf_view_bounds.height());
      }
    }
  }

  // Simulate a mouse press event on the shelf's view at |view_index|.
  views::View* SimulateViewPressed(ShelfView::Pointer pointer, int view_index) {
    views::View* view = test_api_->GetViewAt(view_index);
    ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                 view->GetBoundsInScreen().origin(),
                                 ui::EventTimeForNow(), 0, 0);
    shelf_view_->PointerPressedOnButton(view, pointer, pressed_event);
    return view;
  }

  // Similar to SimulateViewPressed, but the index must not be for the app list,
  // since the app list button is not a ShelfButton.
  ShelfButton* SimulateButtonPressed(ShelfView::Pointer pointer,
                                     int button_index) {
    EXPECT_NE(TYPE_APP_LIST, model_->items()[button_index].type);
    ShelfButton* button = test_api_->GetButton(button_index);
    EXPECT_EQ(button, SimulateViewPressed(pointer, button_index));
    return button;
  }

  // Simulates a single mouse click.
  void SimulateClick(int button_index) {
    ShelfButton* button = SimulateButtonPressed(ShelfView::MOUSE, button_index);
    ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                 button->GetBoundsInScreen().origin(),
                                 ui::EventTimeForNow(), 0, 0);
    shelf_view_->ButtonPressed(
        button, release_event,
        views::test::InkDropHostViewTestApi(button).GetInkDrop());
    shelf_view_->PointerReleasedOnButton(button, ShelfView::MOUSE, false);
  }

  // Simulates the second click of a double click.
  void SimulateDoubleClick(int button_index) {
    ShelfButton* button = SimulateButtonPressed(ShelfView::MOUSE, button_index);
    ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                 button->GetBoundsInScreen().origin(),
                                 ui::EventTimeForNow(), ui::EF_IS_DOUBLE_CLICK,
                                 0);
    shelf_view_->ButtonPressed(
        button, release_event,
        views::test::InkDropHostViewTestApi(button).GetInkDrop());
    shelf_view_->PointerReleasedOnButton(button, ShelfView::MOUSE, false);
  }

  void DoDrag(int dist_x,
              int dist_y,
              views::View* button,
              ShelfView::Pointer pointer,
              views::View* to) {
    ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, gfx::Point(dist_x, dist_y),
                              to->GetBoundsInScreen().origin(),
                              ui::EventTimeForNow(), 0, 0);
    shelf_view_->PointerDraggedOnButton(button, pointer, drag_event);
  }

  /*
   * Trigger ContinueDrag of the shelf
   * The argument progressively means whether to simulate the drag progress (a
   * series of changes of the posistion of dragged item), like the normal user
   * drag behavior.
   */
  void ContinueDrag(views::View* button,
                    ShelfView::Pointer pointer,
                    int from_index,
                    int to_index,
                    bool progressively) {
    views::View* to = test_api_->GetViewAt(to_index);
    views::View* from = test_api_->GetViewAt(from_index);
    int dist_x = to->x() - from->x();
    int dist_y = to->y() - from->y();
    if (progressively) {
      int sgn = dist_x > 0 ? 1 : -1;
      dist_x = abs(dist_x);
      for (; dist_x; dist_x -= std::min(10, dist_x))
        DoDrag(sgn * std::min(10, abs(dist_x)), 0, button, pointer, to);
    } else {
      DoDrag(dist_x, dist_y, button, pointer, to);
    }
  }

  /*
   * Simulate drag operation.
   * Argument progressively means whether to simulate the drag progress (a
   * series of changes of the posistion of dragged item) like the behavior of
   * user drags.
   */
  views::View* SimulateDrag(ShelfView::Pointer pointer,
                            int button_index,
                            int destination_index,
                            bool progressively) {
    views::View* button = SimulateViewPressed(pointer, button_index);

    if (!progressively) {
      ContinueDrag(button, pointer, button_index, destination_index, false);
    } else if (button_index < destination_index) {
      for (int cur_index = button_index + 1; cur_index <= destination_index;
           cur_index++)
        ContinueDrag(button, pointer, cur_index - 1, cur_index, true);
    } else if (button_index > destination_index) {
      for (int cur_index = button_index - 1; cur_index >= destination_index;
           cur_index--)
        ContinueDrag(button, pointer, cur_index + 1, cur_index, true);
    }
    return button;
  }

  void DragAndVerify(
      int from,
      int to,
      ShelfView* shelf_view,
      const std::vector<std::pair<ShelfID, views::View*>>& expected_id_map) {
    views::View* dragged_button =
        SimulateDrag(ShelfView::MOUSE, from, to, true);
    shelf_view->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE,
                                        false);
    test_api_->RunMessageLoopUntilAnimationsDone();
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(expected_id_map));
  }

  void SetupForDragTest(std::vector<std::pair<ShelfID, views::View*>>* id_map) {
    // Initialize |id_map| with the automatically-created shelf buttons.
    for (size_t i = 0; i < model_->items().size(); ++i) {
      ShelfButton* button = test_api_->GetButton(i);
      id_map->push_back(std::make_pair(model_->items()[i].id, button));
    }
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(*id_map));

    // Add 5 app shelf buttons for testing.
    for (int i = 0; i < 5; ++i) {
      ShelfID id = AddAppShortcut();
      // The back button is located at index 0, the app icon is located at
      // index 1, and the browser shortcut is located at index 2. So we should
      // start to add app shortcuts at index 3.
      id_map->insert(id_map->begin() + i + 3,
                     std::make_pair(id, GetButtonByID(id)));
    }
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(*id_map));
  }

  void AddButtonsUntilOverflow() {
    int items_added = 0;
    while (!test_api_->IsOverflowButtonVisible()) {
      AddAppShortcut();
      ++items_added;
      ASSERT_LT(items_added, 10000);
    }
  }

  // Helper function for testing dragging an item off one shelf to another
  // shelf. |main_to_overflow| is true if we are moving the item from the main
  // shelf to the overflow shelf; it is false if we are moving the item from the
  // overflow shelf to the main shelf. |cancel| is true if we want to cancel the
  // dragging halfway through.
  void TestDraggingAnItemFromShelfToOtherShelf(bool main_to_overflow,
                                               bool cancel) {
    test_api_->ShowOverflowBubble();
    ASSERT_TRUE(test_api_->IsShowingOverflowBubble());

    ShelfViewTestAPI test_api_for_overflow(
        test_api_->overflow_bubble()->bubble_view()->shelf_view());

    int total_item_count = model_->item_count();

    // Intialize some ids to test after the drag operation is canceled or
    // completed. These ids are set assuming the both the main shelf and
    // overflow shelf has more than 3 items.
    ShelfID last_visible_item_id_in_shelf =
        GetItemId(test_api_->GetLastVisibleIndex());
    ShelfID second_last_visible_item_id_in_shelf =
        GetItemId(test_api_->GetLastVisibleIndex() - 1);
    ShelfID first_visible_item_id_in_overflow =
        GetItemId(test_api_for_overflow.GetFirstVisibleIndex());
    ShelfID second_last_visible_item_id_in_overflow =
        GetItemId(test_api_for_overflow.GetLastVisibleIndex() - 1);

    // |src_api| represents the test api of the shelf we are moving the item
    // from. |dest_api| represents the test api of the shelf we are moving the
    // item too.
    ShelfViewTestAPI* src_api =
        main_to_overflow ? test_api_.get() : &test_api_for_overflow;
    ShelfViewTestAPI* dest_api =
        main_to_overflow ? &test_api_for_overflow : test_api_.get();

    // Set the item to be dragged depending on |main_to_overflow|.
    int drag_item_index = main_to_overflow ? 3 : src_api->GetLastVisibleIndex();
    ShelfID drag_item_id = GetItemId(drag_item_index);
    ShelfButton* drag_button = src_api->GetButton(drag_item_index);
    gfx::Point center_point_of_drag_item = GetButtonCenter(drag_button);

    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->set_current_location(center_point_of_drag_item);
    // Rip an item off this source shelf.
    generator->PressLeftButton();
    gfx::Point rip_off_point(center_point_of_drag_item.x(), 0);
    generator->MoveMouseTo(rip_off_point);
    src_api->RunMessageLoopUntilAnimationsDone();
    dest_api->RunMessageLoopUntilAnimationsDone();
    ASSERT_TRUE(src_api->IsRippedOffFromShelf());
    ASSERT_FALSE(src_api->DraggedItemToAnotherShelf());

    // Move a dragged item into the destination shelf at |drop_index|.
    int drop_index = main_to_overflow ? dest_api->GetLastVisibleIndex() : 3;
    ShelfButton* drop_button = dest_api->GetButton(drop_index);
    gfx::Point drop_point = GetButtonCenter(drop_button);
    // To insert at |drop_index|, a smaller x-axis value of |drop_point|
    // should be used. If |drop_index| is the last item, a larger x-axis
    // value of |drop_point| should be used.
    int drop_point_x_shift = main_to_overflow
                                 ? ShelfConstants::button_size() / 4
                                 : -ShelfConstants::button_size() / 4;
    gfx::Point modified_drop_point(drop_point.x() + drop_point_x_shift,
                                   drop_point.y());
    generator->MoveMouseTo(modified_drop_point);
    src_api->RunMessageLoopUntilAnimationsDone();
    dest_api->RunMessageLoopUntilAnimationsDone();
    ASSERT_TRUE(src_api->IsRippedOffFromShelf());
    ASSERT_TRUE(src_api->DraggedItemToAnotherShelf());

    if (cancel)
      drag_button->OnMouseCaptureLost();

    generator->ReleaseLeftButton();

    src_api->RunMessageLoopUntilAnimationsDone();
    dest_api->RunMessageLoopUntilAnimationsDone();
    ASSERT_FALSE(src_api->IsRippedOffFromShelf());
    ASSERT_FALSE(src_api->DraggedItemToAnotherShelf());

    // Compare pre-stored items' id with newly positioned items' after dragging
    // is canceled or finished.
    if (cancel) {
      // Item ids should remain unchanged if operation was canceled.
      EXPECT_EQ(last_visible_item_id_in_shelf,
                GetItemId(test_api_->GetLastVisibleIndex()));
      EXPECT_EQ(second_last_visible_item_id_in_shelf,
                GetItemId(test_api_->GetLastVisibleIndex() - 1));
      EXPECT_EQ(first_visible_item_id_in_overflow,
                GetItemId(test_api_for_overflow.GetFirstVisibleIndex()));
      EXPECT_EQ(second_last_visible_item_id_in_overflow,
                GetItemId(test_api_for_overflow.GetLastVisibleIndex() - 1));
    } else {
      EXPECT_EQ(drag_item_id, GetItemId(drop_index));
      EXPECT_EQ(total_item_count, model_->item_count());

      if (main_to_overflow) {
        // If we move an item from the main shelf to the overflow shelf, the
        // following should happen:
        // 1) The former last item on the main shelf should now be the second
        // last item on the main shelf.
        // 2) The former first item on the overflow shelf should now be the last
        // item on the main shelf.
        // 3) The dragged item should now be the last item on the main shelf.
        EXPECT_EQ(last_visible_item_id_in_shelf,
                  GetItemId(test_api_->GetLastVisibleIndex() - 1));
        EXPECT_EQ(first_visible_item_id_in_overflow,
                  GetItemId(test_api_->GetLastVisibleIndex()));
        EXPECT_EQ(drag_item_id,
                  GetItemId(test_api_for_overflow.GetLastVisibleIndex()));
      } else {
        // If we move an item from the overflow shelf to the main shelf, the
        // following should happen:
        // 1) The former last item on the main shelf should now be the first
        // item on the overflow shelf.
        // 2) The former second last item on the main shelf should now be the
        // last item on the main shelf.
        // 3) The former first item on the overflow shelf should now be the
        // second item on the overflow shelf.
        // 4) The former second item on the overflow shelf should now be the
        // last item on the overflow shelf (since there are 3 items on the
        // overflow shelf).
        EXPECT_EQ(last_visible_item_id_in_shelf,
                  GetItemId(test_api_for_overflow.GetFirstVisibleIndex()));
        EXPECT_EQ(second_last_visible_item_id_in_shelf,
                  GetItemId(test_api_->GetLastVisibleIndex()));
        EXPECT_EQ(first_visible_item_id_in_overflow,
                  GetItemId(test_api_for_overflow.GetFirstVisibleIndex() + 1));
        EXPECT_EQ(second_last_visible_item_id_in_overflow,
                  GetItemId(test_api_for_overflow.GetLastVisibleIndex()));
      }
    }
    test_api_->HideOverflowBubble();
  }

  // Returns the item's ShelfID at |index|.
  ShelfID GetItemId(int index) {
    DCHECK_GE(index, 0);
    return model_->items()[index].id;
  }

  // Returns the center point of a button. Helper function for event generators.
  gfx::Point GetButtonCenter(const ShelfID& button_id) {
    return GetButtonCenter(GetButtonByID(button_id));
  }

  gfx::Point GetButtonCenter(ShelfButton* button) {
    return button->GetBoundsInScreen().CenterPoint();
  }

  ShelfModel* model_ = nullptr;
  ShelfView* shelf_view_ = nullptr;
  int id_ = 0;

  std::unique_ptr<ShelfViewTestAPI> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfViewTest);
};

const char*
    ShelfViewTest::kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName =
        ShelfButtonPressedMetricTracker::
            kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName;

class ShelfViewTextDirectionTest : public ShelfViewTest,
                                   public testing::WithParamInterface<bool> {
 public:
  ShelfViewTextDirectionTest() : scoped_locale_(GetParam() ? "he" : "") {}
  virtual ~ShelfViewTextDirectionTest() = default;

 private:
  // Restores locale to the default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;

  DISALLOW_COPY_AND_ASSIGN(ShelfViewTextDirectionTest);
};

// Check the ideal bounds of several items in LTR and RTL UI.
TEST_P(ShelfViewTextDirectionTest, GetIdealBoundsOfItemIcon) {
  ShelfID id_1 = AddAppShortcut();
  AddButtonsUntilOverflow();
  ShelfID id_2 = AddAppShortcut();
  ShelfID id_3 = AddAppShortcut();

  const gfx::Rect bounds_1 = shelf_view_->GetIdealBoundsOfItemIcon(id_1);
  const gfx::Rect bounds_2 = shelf_view_->GetIdealBoundsOfItemIcon(id_2);
  const gfx::Rect bounds_3 = shelf_view_->GetIdealBoundsOfItemIcon(id_3);

  EXPECT_EQ(shelf_view_->GetIdealBoundsOfItemIcon(ShelfID(kAppListId)),
            shelf_view_->GetAppListButton()->GetMirroredBounds());
  EXPECT_EQ(shelf_view_->GetIdealBoundsOfItemIcon(ShelfID(kBackButtonId)),
            shelf_view_->GetBackButton()->GetMirroredBounds());

  // Just items in the overflow area return the overflow button's ideal bounds.
  EXPECT_NE(bounds_1, test_api_->overflow_button()->GetMirroredBounds());
  EXPECT_TRUE(GetButtonByID(id_1)->GetMirroredBounds().Contains(bounds_1));
  EXPECT_EQ(bounds_2, test_api_->overflow_button()->GetMirroredBounds());
  EXPECT_EQ(bounds_3, test_api_->overflow_button()->GetMirroredBounds());
}

// Checks that shelf view contents are considered in the correct drag group.
TEST_F(ShelfViewTest, EnforceDragType) {
  EXPECT_TRUE(test_api_->SameDragType(TYPE_APP, TYPE_APP));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP, TYPE_PINNED_APP));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP, TYPE_BROWSER_SHORTCUT));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP, TYPE_APP_LIST));

  EXPECT_TRUE(test_api_->SameDragType(TYPE_PINNED_APP, TYPE_PINNED_APP));
  EXPECT_TRUE(test_api_->SameDragType(TYPE_PINNED_APP, TYPE_BROWSER_SHORTCUT));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_PINNED_APP, TYPE_APP_LIST));

  EXPECT_TRUE(
      test_api_->SameDragType(TYPE_BROWSER_SHORTCUT, TYPE_BROWSER_SHORTCUT));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_BROWSER_SHORTCUT, TYPE_APP_LIST));

  EXPECT_TRUE(test_api_->SameDragType(TYPE_APP_LIST, TYPE_APP_LIST));
}

// Adds platform app button until overflow and verifies that the last added
// platform app button is hidden.
TEST_F(ShelfViewTest, AddBrowserUntilOverflow) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button until overflow.
  int items_added = 0;
  ShelfID last_added = AddApp();
  while (!test_api_->IsOverflowButtonVisible()) {
    // Added button is visible after animation while in this loop.
    EXPECT_TRUE(GetButtonByID(last_added)->visible());

    last_added = AddApp();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // The last added button should be invisible.
  EXPECT_FALSE(GetButtonByID(last_added)->visible());
}

// Adds one platform app button then adds app shortcut until overflow. Verifies
// that the browser button gets hidden on overflow and last added app shortcut
// is still visible.
TEST_F(ShelfViewTest, AddAppShortcutWithBrowserButtonUntilOverflow) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  ShelfID browser_button_id = AddApp();

  // Add app shortcut until overflow.
  int items_added = 0;
  ShelfID last_added = AddAppShortcut();
  while (!test_api_->IsOverflowButtonVisible()) {
    // Added button is visible after animation while in this loop.
    EXPECT_TRUE(GetButtonByID(last_added)->visible());

    last_added = AddAppShortcut();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // And the platform app button is invisible.
  EXPECT_FALSE(GetButtonByID(browser_button_id)->visible());
}

// Making sure that no buttons on the shelf will ever overlap after adding many
// of them.
TEST_F(ShelfViewTest, AssertNoButtonsOverlap) {
  std::vector<ShelfID> button_ids;
  // Add app icons until the overflow button is visible.
  while (!test_api_->IsOverflowButtonVisible()) {
    ShelfID id = AddApp();
    button_ids.push_back(id);
  }
  ASSERT_LT(button_ids.size(), 10000U);
  ASSERT_GT(button_ids.size(), 2U);

  // Remove 2 icons to make more room, the overflow button should go away.
  for (int i = 0; i < 2; ++i) {
    ShelfID id = button_ids.back();
    RemoveByID(id);
    button_ids.pop_back();
  }
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
  EXPECT_TRUE(GetButtonByID(button_ids.back())->visible());

  // Add 20 app icons, and expect to have overflow.
  for (int i = 0; i < 20; ++i) {
    ShelfID id = AddAppShortcut();
    button_ids.push_back(id);
  }
  ASSERT_LT(button_ids.size(), 10000U);
  EXPECT_TRUE(test_api_->IsOverflowButtonVisible());

  // Test that any two successive visible icons never overlap in all shelf
  // alignment types.
  const ShelfAlignment kAlignments[] = {
      SHELF_ALIGNMENT_LEFT, SHELF_ALIGNMENT_RIGHT, SHELF_ALIGNMENT_BOTTOM,
      SHELF_ALIGNMENT_BOTTOM_LOCKED,
  };

  for (ShelfAlignment alignment : kAlignments) {
    shelf_view_->shelf()->SetAlignment(alignment);
    // For every 2 successive visible icons, expect that their bounds don't
    // intersect.
    for (int i = 2; i < test_api_->GetButtonCount() - 1; ++i) {
      if (!(test_api_->GetButton(i)->visible() &&
            test_api_->GetButton(i + 1)->visible())) {
        continue;
      }

      const gfx::Rect& bounds1 = test_api_->GetBoundsByIndex(i);
      const gfx::Rect& bounds2 = test_api_->GetBoundsByIndex(i + 1);
      EXPECT_FALSE(bounds1.Intersects(bounds2));
    }
  }
}

// Adds button until overflow then removes first added one. Verifies that
// the last added one changes from invisible to visible and overflow
// chevron is gone.
TEST_F(ShelfViewTest, RemoveButtonRevealsOverflowed) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add platform app buttons until overflow.
  int items_added = 0;
  ShelfID first_added = AddApp();
  ShelfID last_added = first_added;
  while (!test_api_->IsOverflowButtonVisible()) {
    last_added = AddApp();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // Expect add more than 1 button. First added is visible and last is not.
  EXPECT_NE(first_added, last_added);
  EXPECT_TRUE(GetButtonByID(first_added)->visible());
  EXPECT_FALSE(GetButtonByID(last_added)->visible());

  // Remove first added.
  RemoveByID(first_added);

  // Last added button becomes visible and overflow chevron is gone.
  EXPECT_TRUE(GetButtonByID(last_added)->visible());
  EXPECT_EQ(1.0f, GetButtonByID(last_added)->layer()->opacity());
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
}

// Verifies that remove last overflowed button should hide overflow chevron.
TEST_F(ShelfViewTest, RemoveLastOverflowed) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button until overflow.
  int items_added = 0;
  ShelfID last_added = AddApp();
  while (!test_api_->IsOverflowButtonVisible()) {
    last_added = AddApp();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  RemoveByID(last_added);
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
}

// Tests the visiblity of certain shelf items when the overflow bubble is open
// and entering or exiting tablet mode.
TEST_F(ShelfViewTest, OverflowVisibleItemsInTabletMode) {
  // Helper to check whether the item with index |index| is visible on the shelf
  // associated with |shelf_test_api|.
  auto is_visible_on_shelf = [](int index, ShelfViewTestAPI* shelf_test_api) {
    return shelf_test_api->shelf_view()
        ->view_model_for_test()
        ->view_at(index)
        ->visible();
  };

  // Setup the shelf so the overflow bubble is visible.
  AddButtonsUntilOverflow();
  test_api_->ShowOverflowBubble();
  ShelfViewTestAPI overflow_test_api(
      test_api_->overflow_bubble()->bubble_view()->shelf_view());

  // The main shelf is currently showing the item at |last_visible_index|.
  const int last_visible_index = test_api_->GetLastVisibleIndex();
  EXPECT_TRUE(is_visible_on_shelf(last_visible_index, test_api_.get()));
  EXPECT_FALSE(is_visible_on_shelf(last_visible_index, &overflow_test_api));

  // Verify that after entering tablet mode, the last item on the main shelf
  // is no longer visible on the main shelf but is now visible on the overflow
  // shelf, due to the back button taking up space on the main shelf. The shelf
  // model and corresponding view should be updated to reflect this.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  test_api_->RunMessageLoopUntilAnimationsDone();
  overflow_test_api.RunMessageLoopUntilAnimationsDone();
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
  // TODO(manucornet): Parts of this test fail with the new UI. Find out why
  // and re-enable. https://crbug.com/891080
  // EXPECT_FALSE(is_visible_on_shelf(last_visible_index, test_api_.get()));
  // EXPECT_TRUE(is_visible_on_shelf(last_visible_index, &overflow_test_api));

  // Verify that the item at |last_visible_index| is once again shown on the
  // main shelf after exiting tablet mode.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  test_api_->RunMessageLoopUntilAnimationsDone();
  overflow_test_api.RunMessageLoopUntilAnimationsDone();
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
  EXPECT_TRUE(is_visible_on_shelf(last_visible_index, test_api_.get()));
  EXPECT_FALSE(is_visible_on_shelf(last_visible_index, &overflow_test_api));
}

// Adds platform app button without waiting for animation to finish and verifies
// that all added buttons are visible.
TEST_F(ShelfViewTest, AddButtonQuickly) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add a few platform buttons quickly without wait for animation.
  int added_count = 0;
  while (!test_api_->IsOverflowButtonVisible()) {
    AddAppNoWait();
    ++added_count;
    ASSERT_LT(added_count, 10000);
  }

  // ShelfView should be big enough to hold at least 3 new buttons.
  ASSERT_GE(added_count, 3);

  // Wait for the last animation to finish.
  test_api_->RunMessageLoopUntilAnimationsDone();

  // Verifies non-overflow buttons are visible. The back button at index 0 is
  // not visible.
  for (int i = 1; i <= test_api_->GetLastVisibleIndex(); ++i) {
    ShelfButton* button = test_api_->GetButton(i);
    if (button) {
      EXPECT_TRUE(button->visible()) << "button index=" << i;
      EXPECT_EQ(1.0f, button->layer()->opacity()) << "button index=" << i;
    }
  }
}

// Check that model changes are handled correctly while a shelf icon is being
// dragged.
TEST_F(ShelfViewTest, ModelChangesWhileDragging) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  // Dragging browser shortcut at index 1.
  EXPECT_TRUE(model_->items()[2].type == TYPE_BROWSER_SHORTCUT);
  views::View* dragged_button = SimulateDrag(ShelfView::MOUSE, 2, 4, false);
  std::rotate(id_map.begin() + 2, id_map.begin() + 3, id_map.begin() + 5);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  EXPECT_TRUE(model_->items()[4].type == TYPE_BROWSER_SHORTCUT);

  // Dragging changes model order.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 2, 4, false);
  std::rotate(id_map.begin() + 2, id_map.begin() + 3, id_map.begin() + 5);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Cancelling the drag operation restores previous order.
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, true);
  std::rotate(id_map.begin() + 2, id_map.begin() + 4, id_map.begin() + 5);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Deleting an item keeps the remaining intact.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 2, 4, false);
  model_->RemoveItemAt(2);
  id_map.erase(id_map.begin() + 2);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);

  // Adding a shelf item cancels the drag and respects the order.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 2, 4, false);
  ShelfID new_id = AddAppShortcut();
  id_map.insert(id_map.begin() + 7,
                std::make_pair(new_id, GetButtonByID(new_id)));
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
}

// Check that 2nd drag from the other pointer would be ignored.
TEST_F(ShelfViewTest, SimultaneousDrag) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  // Start a mouse drag.
  views::View* dragged_button_mouse =
      SimulateDrag(ShelfView::MOUSE, 2, 4, false);
  std::rotate(id_map.begin() + 2, id_map.begin() + 3, id_map.begin() + 5);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  // Attempt a touch drag before the mouse drag finishes.
  views::View* dragged_button_touch =
      SimulateDrag(ShelfView::TOUCH, 5, 3, false);

  // Nothing changes since 2nd drag is ignored.
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Finish the mouse drag.
  shelf_view_->PointerReleasedOnButton(dragged_button_mouse, ShelfView::MOUSE,
                                       false);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Now start a touch drag.
  dragged_button_touch = SimulateDrag(ShelfView::TOUCH, 5, 3, false);
  std::rotate(id_map.begin() + 4, id_map.begin() + 5, id_map.begin() + 6);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // And attempt a mouse drag before the touch drag finishes.
  dragged_button_mouse = SimulateDrag(ShelfView::MOUSE, 2, 3, false);

  // Nothing changes since 2nd drag is ignored.
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  shelf_view_->PointerReleasedOnButton(dragged_button_touch, ShelfView::TOUCH,
                                       false);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
}

// Ensure the app list button cannot be dragged and other items cannot be
// dragged in front of the back button or app list button.
TEST_F(ShelfViewTest, DragWithNotDraggableItemInFront) {
  // The expected id order is initialized as: 1, 2, 3, 4, 5, 6, 7
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  // Ensure that the back button cannot be dragged.
  // The expected id order is unchanged: 1, 2, 3, 4, 5, 6, 7
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(0, 1, shelf_view_, id_map));
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(0, 2, shelf_view_, id_map));
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(0, 5, shelf_view_, id_map));

  // Ensure that the app list button cannot be dragged.
  // The expected id order is unchanged: 1, 2, 3, 4, 5, 6, 7
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(1, 1, shelf_view_, id_map));
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(1, 2, shelf_view_, id_map));
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(1, 5, shelf_view_, id_map));

  // Ensure that items cannot be dragged in front of the back button.
  // Attempting to do so will order buttons immediately after the app list.
  // Dragging the third button in front should no-op: 1, 2, 3, 4, 5, 6, 7
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(2, 0, shelf_view_, id_map));
  // Dragging the fourth button in front should yield: 1, 2, 4, 3, 5, 6, 7
  std::rotate(id_map.begin() + 2, id_map.begin() + 3, id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(3, 0, shelf_view_, id_map));
  // Dragging the sixth button in front should yield: 1, 2, 6, 4, 3, 5, 7
  std::rotate(id_map.begin() + 2, id_map.begin() + 5, id_map.begin() + 6);
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(5, 0, shelf_view_, id_map));

  // Ensure that items cannot be dragged in front of the app list button.
  // Attempting to do so will order buttons immediately after the app list.
  // Dragging the third button in front should no-op: 1, 2, 6, 4, 3, 5, 7
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(2, 1, shelf_view_, id_map));
  // Dragging the fourth button in front should yield: 1, 2, 4, 6, 3, 5, 7
  std::rotate(id_map.begin() + 2, id_map.begin() + 3, id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(3, 1, shelf_view_, id_map));
  // Dragging the sixth button in front should yield: 1, 2, 5, 4, 6, 3, 7
  std::rotate(id_map.begin() + 2, id_map.begin() + 5, id_map.begin() + 6);
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(5, 1, shelf_view_, id_map));
}

// Ensure that clicking on one item and then dragging another works as expected.
TEST_F(ShelfViewTest, ClickOneDragAnother) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  // A click on the item at index 2 is simulated.
  SimulateClick(3);

  // Dragging the browser item at index 1 should change the model order.
  EXPECT_TRUE(model_->items()[2].type == TYPE_BROWSER_SHORTCUT);
  views::View* dragged_button = SimulateDrag(ShelfView::MOUSE, 2, 4, false);
  std::rotate(id_map.begin() + 2, id_map.begin() + 3, id_map.begin() + 5);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  EXPECT_TRUE(model_->items()[4].type == TYPE_BROWSER_SHORTCUT);
}

// Tests that double-clicking an item does not activate it twice.
TEST_F(ShelfViewTest, ClickingTwiceActivatesOnce) {
  // Watch for selection of the browser shortcut.
  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->SetShelfItemDelegate(model_->items()[2].id,
                               base::WrapUnique(selection_tracker));

  // A single click selects the item, but a double-click does not.
  EXPECT_EQ(0u, selection_tracker->item_selected_count());
  SimulateClick(2);
  EXPECT_EQ(1u, selection_tracker->item_selected_count());
  SimulateDoubleClick(2);
  EXPECT_EQ(1u, selection_tracker->item_selected_count());
}

// Check that very small mouse drags do not prevent shelf item selection.
TEST_F(ShelfViewTest, ClickAndMoveSlightly) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  ShelfID shelf_id = (id_map.begin() + 2)->first;
  views::View* button = (id_map.begin() + 2)->second;

  // Install a ShelfItemDelegate that tracks when the shelf item is selected.
  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->SetShelfItemDelegate(
      shelf_id, base::WrapUnique<ShelfItemSelectionTracker>(selection_tracker));

  gfx::Vector2d press_offset(5, 30);
  gfx::Point press_location = gfx::Point() + press_offset;
  gfx::Point press_location_in_screen =
      button->GetBoundsInScreen().origin() + press_offset;

  ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, press_location,
                             press_location_in_screen, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(click_event);
  EXPECT_EQ(0u, selection_tracker->item_selected_count());

  ui::MouseEvent drag_event1(
      ui::ET_MOUSE_DRAGGED, press_location + gfx::Vector2d(0, 1),
      press_location_in_screen + gfx::Vector2d(0, 1), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event1);
  ui::MouseEvent drag_event2(
      ui::ET_MOUSE_DRAGGED, press_location + gfx::Vector2d(-1, 0),
      press_location_in_screen + gfx::Vector2d(-1, 0), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event2);
  EXPECT_EQ(0u, selection_tracker->item_selected_count());

  ui::MouseEvent release_event(
      ui::ET_MOUSE_RELEASED, press_location + gfx::Vector2d(-1, 0),
      press_location_in_screen + gfx::Vector2d(-1, 0), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
  EXPECT_EQ(1u, selection_tracker->item_selected_count());
}

// Confirm that item status changes are reflected in the buttons.
TEST_F(ShelfViewTest, ShelfItemStatus) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button.
  ShelfID last_added = AddApp();
  ShelfItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  ShelfButton* button = GetButtonByID(last_added);
  ASSERT_EQ(ShelfButton::STATE_RUNNING, button->state());
  item.status = STATUS_ATTENTION;
  model_->Set(index, item);
  ASSERT_EQ(ShelfButton::STATE_ATTENTION, button->state());
}

// Test what drag movements will rip an item off the shelf.
TEST_F(ShelfViewTest, ShelfRipOff) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // The test makes some assumptions that the shelf is bottom aligned.
  ASSERT_EQ(shelf_view_->shelf()->alignment(), SHELF_ALIGNMENT_BOTTOM);

  // The rip off threshold. Taken from |kRipOffDistance| in shelf_view.cc.
  const int kRipOffDistance = 48;

  // Add two apps (which is on the main shelf) and then add buttons until
  // overflow. Add one more app (which is on the overflow shelf).
  ShelfID first_app_id = AddAppShortcut();
  ShelfID second_app_id = AddAppShortcut();
  AddButtonsUntilOverflow();
  ShelfID overflow_app_id = AddAppShortcut();

  // Verify that dragging an app off the shelf will trigger the app getting
  // ripped off, unless the distance is less than |kRipOffDistance|.
  gfx::Point first_app_location = GetButtonCenter(GetButtonByID(first_app_id));
  generator->set_current_location(first_app_location);
  generator->PressLeftButton();
  // Drag the mouse to just off the shelf.
  generator->MoveMouseBy(0, -ShelfConstants::shelf_size() / 2 - 1);
  EXPECT_FALSE(test_api_->IsRippedOffFromShelf());
  // Drag the mouse past the rip off threshold.
  generator->MoveMouseBy(0, -kRipOffDistance);
  EXPECT_TRUE(test_api_->IsRippedOffFromShelf());
  // Drag the mouse back to the original position, so that the app does not get
  // deleted.
  generator->MoveMouseTo(first_app_location);
  generator->ReleaseLeftButton();
  EXPECT_FALSE(test_api_->IsRippedOffFromShelf());

  // Open overflow shelf and test api for it.
  test_api_->ShowOverflowBubble();
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
  ShelfViewTestAPI test_api_for_overflow(
      test_api_->overflow_bubble()->bubble_view()->shelf_view());

  // Verify that when an app from the main shelf is dragged to a location on the
  // overflow shelf, it is ripped off.
  gfx::Point second_app_location =
      GetButtonCenter(GetButtonByID(second_app_id));
  gfx::Point overflow_app_location = GetButtonCenter(
      test_api_for_overflow.GetButton(model_->ItemIndexByID(overflow_app_id)));
  generator->set_current_location(second_app_location);
  generator->PressLeftButton();
  generator->MoveMouseTo(overflow_app_location);
  EXPECT_TRUE(test_api_->IsRippedOffFromShelf());
  generator->MoveMouseTo(second_app_location);
  generator->ReleaseLeftButton();
  EXPECT_FALSE(test_api_->IsRippedOffFromShelf());

  // Verify that when an app from the overflow shelf is dragged to a location on
  // the main shelf, it is ripped off.
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
  generator->set_current_location(overflow_app_location);
  generator->PressLeftButton();
  generator->MoveMouseTo(second_app_location);
  EXPECT_TRUE(test_api_for_overflow.IsRippedOffFromShelf());
  generator->MoveMouseTo(overflow_app_location);
  generator->ReleaseLeftButton();
  EXPECT_FALSE(test_api_for_overflow.IsRippedOffFromShelf());
}

// Tests that drag and drop a pinned running app will unpin it.
TEST_F(ShelfViewTest, DragAndDropPinnedRunningApp) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // The test makes some assumptions that the shelf is bottom aligned.
  ASSERT_EQ(shelf_view_->shelf()->alignment(), SHELF_ALIGNMENT_BOTTOM);

  // The rip off threshold. Taken from |kRipOffDistance| in shelf_view.cc.
  constexpr int kRipOffDistance = 48;

  const ShelfID id = AddApp();
  // Added only one app here, the index of the app will not change after drag
  // and drop.
  int index = model_->ItemIndexByID(id);
  ShelfItem item = GetItemByID(id);
  EXPECT_EQ(STATUS_RUNNING, item.status);
  PinAppWithID(id);
  EXPECT_TRUE(IsAppPinned(GetItemId(index)));

  gfx::Point app_location = GetButtonCenter(GetButtonByID(id));
  generator->set_current_location(app_location);
  generator->PressLeftButton();
  generator->MoveMouseBy(0, -ShelfConstants::shelf_size() / 2 - 1);
  EXPECT_FALSE(test_api_->IsRippedOffFromShelf());
  generator->MoveMouseBy(0, -kRipOffDistance);
  EXPECT_TRUE(test_api_->IsRippedOffFromShelf());
  generator->ReleaseLeftButton();
  EXPECT_FALSE(IsAppPinned(GetItemId(index)));
}

// Confirm that item status changes are reflected in the buttons
// for platform apps.
TEST_F(ShelfViewTest, ShelfItemStatusPlatformApp) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button.
  ShelfID last_added = AddApp();
  ShelfItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  ShelfButton* button = GetButtonByID(last_added);
  ASSERT_EQ(ShelfButton::STATE_RUNNING, button->state());
  item.status = STATUS_ATTENTION;
  model_->Set(index, item);
  ASSERT_EQ(ShelfButton::STATE_ATTENTION, button->state());
}

// Confirm that shelf item bounds are correctly updated on shelf changes.
TEST_F(ShelfViewTest, ShelfItemBoundsCheck) {
  VerifyShelfItemBoundsAreValid();
  shelf_view_->shelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyShelfItemBoundsAreValid();
  shelf_view_->shelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyShelfItemBoundsAreValid();
}

TEST_F(ShelfViewTest, ShelfTooltipTest) {
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1, test_api_->GetButtonCount());

  // Prepare some items to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfID platform_button_id = AddApp();

  ShelfButton* app_button = GetButtonByID(app_button_id);
  ShelfButton* platform_button = GetButtonByID(platform_button_id);

  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();
  EXPECT_TRUE(shelf_view_->GetWidget()->GetNativeWindow());
  ui::test::EventGenerator* generator = GetEventGenerator();

  generator->MoveMouseTo(app_button->GetBoundsInScreen().CenterPoint());
  // There's a delay to show the tooltip, so it's not visible yet.
  EXPECT_FALSE(tooltip_manager->IsVisible());
  EXPECT_EQ(nullptr, tooltip_manager->GetCurrentAnchorView());

  tooltip_manager->ShowTooltip(app_button);
  EXPECT_TRUE(tooltip_manager->IsVisible());
  EXPECT_EQ(app_button, tooltip_manager->GetCurrentAnchorView());

  // The tooltip will continue showing while the cursor moves between buttons.
  const gfx::Point midpoint =
      gfx::UnionRects(app_button->GetBoundsInScreen(),
                      platform_button->GetBoundsInScreen())
          .CenterPoint();
  generator->MoveMouseTo(midpoint);
  EXPECT_TRUE(tooltip_manager->IsVisible());
  EXPECT_EQ(app_button, tooltip_manager->GetCurrentAnchorView());

  // When the cursor moves over another item, its tooltip shows immediately.
  generator->MoveMouseTo(platform_button->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(tooltip_manager->IsVisible());
  EXPECT_EQ(platform_button, tooltip_manager->GetCurrentAnchorView());
  tooltip_manager->Close();

  // Now cursor over the app_button and move immediately to the platform_button.
  generator->MoveMouseTo(app_button->GetBoundsInScreen().CenterPoint());
  generator->MoveMouseTo(midpoint);
  generator->MoveMouseTo(platform_button->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(tooltip_manager->IsVisible());
  EXPECT_EQ(nullptr, tooltip_manager->GetCurrentAnchorView());
}

// Verify a fix for crash caused by a tooltip update for a deleted shelf
// button, see crbug.com/288838.
TEST_F(ShelfViewTest, RemovingItemClosesTooltip) {
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();

  // Add an item to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfButton* app_button = GetButtonByID(app_button_id);

  // Spawn a tooltip on that item.
  tooltip_manager->ShowTooltip(app_button);
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Remove the app shortcut while the tooltip is open. The tooltip should be
  // closed.
  RemoveByID(app_button_id);
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Change the shelf layout. This should not crash.
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_LEFT);
}

// Changing the shelf alignment closes any open tooltip.
TEST_F(ShelfViewTest, ShelfAlignmentClosesTooltip) {
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();

  // Add an item to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfButton* app_button = GetButtonByID(app_button_id);

  // Spawn a tooltip on the item.
  tooltip_manager->ShowTooltip(app_button);
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Changing shelf alignment hides the tooltip.
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_LEFT);
  EXPECT_FALSE(tooltip_manager->IsVisible());
}

TEST_F(ShelfViewTest, ShouldHideTooltipTest) {
  ShelfID app_button_id = AddAppShortcut();
  ShelfID platform_button_id = AddApp();

  // The tooltip shouldn't hide if the mouse is on normal buttons.
  for (int i = 0; i < test_api_->GetButtonCount(); i++) {
    ShelfButton* button = test_api_->GetButton(i);
    if (!button)
      continue;

    EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
        button->GetMirroredBounds().CenterPoint()))
        << "ShelfView tries to hide on button " << i;
  }

  // The tooltip should not hide on the app-list button.
  AppListButton* app_list_button = shelf_view_->GetAppListButton();
  EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
      app_list_button->GetMirroredBounds().CenterPoint()));

  // The tooltip shouldn't hide if the mouse is in the gap between two buttons.
  gfx::Rect app_button_rect = GetButtonByID(app_button_id)->GetMirroredBounds();
  gfx::Rect platform_button_rect =
      GetButtonByID(platform_button_id)->GetMirroredBounds();
  ASSERT_FALSE(app_button_rect.Intersects(platform_button_rect));
  EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
      gfx::UnionRects(app_button_rect, platform_button_rect).CenterPoint()));

  // The tooltip should hide if it's outside of all buttons.
  gfx::Rect all_area;
  for (int i = 0; i < test_api_->GetButtonCount(); i++) {
    ShelfButton* button = test_api_->GetButton(i);
    if (!button)
      continue;

    all_area.Union(button->GetMirroredBounds());
  }
  all_area.Union(shelf_view_->GetAppListButton()->GetMirroredBounds());
  EXPECT_FALSE(shelf_view_->ShouldHideTooltip(all_area.origin()));
  EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.right() - 1, all_area.bottom() - 1)));
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.right(), all_area.y())));
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.x() - 1, all_area.y())));
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.x(), all_area.y() - 1)));
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.x(), all_area.bottom())));
}

// Test that shelf button tooltips show (except app list) with an open app list.
TEST_F(ShelfViewTest, ShouldHideTooltipWithAppListWindowTest) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // The tooltip shouldn't hide if the mouse is on normal buttons.
  for (int i = 2; i < test_api_->GetButtonCount(); i++) {
    ShelfButton* button = test_api_->GetButton(i);
    if (!button)
      continue;

    EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
        button->GetMirroredBounds().CenterPoint()))
        << "ShelfView tries to hide on button " << i;
  }

  // The tooltip should hide on the app list button if the app list is visible.
  AppListButton* app_list_button = shelf_view_->GetAppListButton();
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      app_list_button->GetMirroredBounds().CenterPoint()));
}

// Test that by moving the mouse cursor off the button onto the bubble it closes
// the bubble.
TEST_F(ShelfViewTest, ShouldHideTooltipWhenHoveringOnTooltip) {
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();
  tooltip_manager->set_timer_delay_for_test(0);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Move the mouse off any item and check that no tooltip is shown.
  generator->MoveMouseTo(gfx::Point(0, 0));
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Move the mouse over the button and check that it is visible.
  AppListButton* app_list_button = shelf_view_->GetAppListButton();
  gfx::Rect bounds = app_list_button->GetBoundsInScreen();
  generator->MoveMouseTo(bounds.CenterPoint());
  // Wait for the timer to go off.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Move the mouse cursor slightly to the right of the item. The tooltip should
  // stay open.
  generator->MoveMouseBy(bounds.width() / 2 + 5, 0);
  // Make sure there is no delayed close.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Move back - it should still stay open.
  generator->MoveMouseBy(-(bounds.width() / 2 + 5), 0);
  // Make sure there is no delayed close.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Now move the mouse cursor slightly above the item - so that it is over the
  // tooltip bubble. Now it should disappear.
  generator->MoveMouseBy(0, -(bounds.height() / 2 + 5));
  // Wait until the delayed close kicked in.
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(tooltip_manager->IsVisible());
}

// Resizing shelf view while an add animation without fade-in is running,
// which happens when overflow happens. App list button should end up in its
// new ideal bounds.
TEST_F(ShelfViewTest, ResizeDuringOverflowAddAnimation) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add buttons until overflow. Let the non-overflow add animations finish but
  // leave the last running.
  int items_added = 0;
  AddAppNoWait();
  while (!test_api_->IsOverflowButtonVisible()) {
    test_api_->RunMessageLoopUntilAnimationsDone();
    AddAppNoWait();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // Resize shelf view with that animation running and stay overflown.
  gfx::Rect bounds = shelf_view_->bounds();
  bounds.set_width(bounds.width() - ShelfConstants::shelf_size());
  shelf_view_->SetBoundsRect(bounds);
  ASSERT_TRUE(test_api_->IsOverflowButtonVisible());

  // Finish the animation.
  test_api_->RunMessageLoopUntilAnimationsDone();

  // App list button should ends up in its new ideal bounds.
  const int app_list_button_index = test_api_->GetButtonCount() - 1;
  const gfx::Rect& app_list_ideal_bounds =
      test_api_->GetIdealBoundsByIndex(app_list_button_index);
  const gfx::Rect& app_list_bounds =
      test_api_->GetBoundsByIndex(app_list_button_index);
  EXPECT_EQ(app_list_ideal_bounds, app_list_bounds);
}

// Checks the overflow bubble size when an item is ripped off and re-inserted.
TEST_F(ShelfViewTest, OverflowBubbleSize) {
  AddButtonsUntilOverflow();
  // Add one more button to prevent the overflow bubble to disappear upon
  // dragging an item out on windows (flakiness, see crbug.com/436131).
  AddAppShortcut();

  // Show overflow bubble.
  test_api_->ShowOverflowBubble();
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());

  ShelfViewTestAPI test_for_overflow_view(
      test_api_->overflow_bubble()->bubble_view()->shelf_view());

  int ripped_index = test_for_overflow_view.GetLastVisibleIndex();
  gfx::Size bubble_size =
      test_for_overflow_view.shelf_view()->GetPreferredSize();
  int item_width =
      ShelfConstants::button_size() + ShelfConstants::button_spacing();

  ui::test::EventGenerator* generator = GetEventGenerator();
  ShelfButton* button = test_for_overflow_view.GetButton(ripped_index);
  // Rip off the last visible item.
  gfx::Point start_point = button->GetBoundsInScreen().CenterPoint();
  gfx::Point rip_off_point(start_point.x(), 0);
  generator->MoveMouseTo(start_point.x(), start_point.y());
  base::RunLoop().RunUntilIdle();
  generator->PressLeftButton();
  base::RunLoop().RunUntilIdle();
  generator->MoveMouseTo(rip_off_point.x(), rip_off_point.y());
  base::RunLoop().RunUntilIdle();
  test_for_overflow_view.RunMessageLoopUntilAnimationsDone();

  // Check the overflow bubble size when an item is ripped off.
  EXPECT_EQ(bubble_size.width() - item_width,
            test_for_overflow_view.shelf_view()->GetPreferredSize().width());
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());

  // Re-insert an item into the overflow bubble.
  int first_index = test_for_overflow_view.GetFirstVisibleIndex();
  button = test_for_overflow_view.GetButton(first_index);

  // Check the bubble size after an item is re-inserted.
  generator->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  test_for_overflow_view.RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(bubble_size.width(),
            test_for_overflow_view.shelf_view()->GetPreferredSize().width());

  generator->ReleaseLeftButton();
  test_for_overflow_view.RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(bubble_size.width(),
            test_for_overflow_view.shelf_view()->GetPreferredSize().width());
}

TEST_F(ShelfViewTest, OverflowShelfColorIsDerivedFromWallpaper) {
  WallpaperControllerTestApi wallpaper_test_api(
      Shell::Get()->wallpaper_controller());
  const SkColor opaque_expected_color =
      wallpaper_test_api.ApplyColorProducingWallpaper();

  AddButtonsUntilOverflow();
  test_api_->ShowOverflowBubble();
  OverflowBubbleView* bubble_view = test_api_->overflow_bubble()->bubble_view();

  EXPECT_EQ(opaque_expected_color, SkColorSetA(bubble_view->color(), 255));
}

// Check the drag insertion bounds of scrolled overflow bubble.
TEST_F(ShelfViewTest, CheckDragInsertBoundsOfScrolledOverflowBubble) {
  UpdateDisplay("400x300");

  AddButtonsUntilOverflow();

  // Show overflow bubble.
  test_api_->ShowOverflowBubble();
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());

  int item_width =
      ShelfConstants::button_size() + ShelfConstants::button_spacing();
  OverflowBubbleView* bubble_view = test_api_->overflow_bubble()->bubble_view();
  OverflowBubbleViewTestAPI bubble_view_api(bubble_view);

  // Add more buttons until OverflowBubble is scrollable and it has 3 invisible
  // items.
  while (bubble_view_api.GetContentsSize().width() <
         (bubble_view->GetContentsBounds().width() + 3 * item_width)) {
    AddAppShortcut();
  }

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());

  ShelfViewTestAPI test_for_overflow_view(
      test_api_->overflow_bubble()->bubble_view()->shelf_view());
  int first_index = test_for_overflow_view.GetFirstVisibleIndex();
  int last_index = test_for_overflow_view.GetLastVisibleIndex();

  ShelfButton* first_button = test_for_overflow_view.GetButton(first_index);
  ShelfButton* last_button = test_for_overflow_view.GetButton(last_index);
  gfx::Point first_point = first_button->GetBoundsInScreen().CenterPoint();
  gfx::Point last_point = last_button->GetBoundsInScreen().CenterPoint();
  gfx::Rect drag_reinsert_bounds =
      test_for_overflow_view.GetBoundsForDragInsertInScreen();
  EXPECT_TRUE(drag_reinsert_bounds.Contains(first_point));
  EXPECT_FALSE(drag_reinsert_bounds.Contains(last_point));

  // Scroll sufficiently to completely show last item.
  bubble_view_api.ScrollByXOffset(bubble_view_api.GetContentsSize().width() -
                                  bubble_view->GetContentsBounds().width());
  drag_reinsert_bounds =
      test_for_overflow_view.GetBoundsForDragInsertInScreen();
  first_point = first_button->GetBoundsInScreen().CenterPoint();
  last_point = last_button->GetBoundsInScreen().CenterPoint();
  EXPECT_FALSE(drag_reinsert_bounds.Contains(first_point));
  EXPECT_TRUE(drag_reinsert_bounds.Contains(last_point));
}

// Check the drag insertion bounds of shelf view in multi monitor environment.
TEST_F(ShelfViewTest, CheckDragInsertBoundsWithMultiMonitor) {
  UpdateDisplay("800x600,800x600");
  Shelf* secondary_shelf = Shelf::ForWindow(Shell::GetAllRootWindows()[1]);
  ShelfView* shelf_view_for_secondary =
      secondary_shelf->GetShelfViewForTesting();

  // The bounds should be big enough for 4 buttons + overflow chevron.
  shelf_view_for_secondary->SetBounds(0, 0, 500, ShelfConstants::shelf_size());

  ShelfViewTestAPI test_api_for_secondary(shelf_view_for_secondary);
  // Speeds up animation for test.
  test_api_for_secondary.SetAnimationDuration(1);

  AddButtonsUntilOverflow();

  // Test #1: Test drag insertion bounds of primary shelf.
  // Show overflow bubble.
  test_api_->ShowOverflowBubble();
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());

  ShelfViewTestAPI test_api_for_overflow_view(
      test_api_->overflow_bubble()->bubble_view()->shelf_view());

  ShelfButton* button = test_api_for_overflow_view.GetButton(
      test_api_for_overflow_view.GetLastVisibleIndex());

  // Checks that a point in shelf is contained in drag insert bounds.
  gfx::Point point_in_shelf_view = button->GetBoundsInScreen().CenterPoint();
  gfx::Rect drag_reinsert_bounds =
      test_api_for_overflow_view.GetBoundsForDragInsertInScreen();
  EXPECT_TRUE(drag_reinsert_bounds.Contains(point_in_shelf_view));
  // Checks that a point out of shelf is not contained in drag insert bounds.
  EXPECT_FALSE(
      drag_reinsert_bounds.Contains(gfx::Point(point_in_shelf_view.x(), 0)));

  // Test #2: Test drag insertion bounds of secondary shelf.
  // Show overflow bubble.
  test_api_for_secondary.ShowOverflowBubble();
  ASSERT_TRUE(test_api_for_secondary.IsShowingOverflowBubble());

  ShelfViewTestAPI test_api_for_overflow_view_of_secondary(
      test_api_for_secondary.overflow_bubble()->bubble_view()->shelf_view());

  ShelfButton* button_in_secondary =
      test_api_for_overflow_view_of_secondary.GetButton(
          test_api_for_overflow_view_of_secondary.GetLastVisibleIndex());

  // Checks that a point in shelf is contained in drag insert bounds.
  gfx::Point point_in_secondary_shelf_view =
      button_in_secondary->GetBoundsInScreen().CenterPoint();
  gfx::Rect drag_reinsert_bounds_in_secondary =
      test_api_for_overflow_view_of_secondary.GetBoundsForDragInsertInScreen();
  EXPECT_TRUE(drag_reinsert_bounds_in_secondary.Contains(
      point_in_secondary_shelf_view));
  // Checks that a point out of shelf is not contained in drag insert bounds.
  EXPECT_FALSE(drag_reinsert_bounds_in_secondary.Contains(
      gfx::Point(point_in_secondary_shelf_view.x(), 0)));
  // Checks that a point of overflow bubble in primary shelf should not be
  // contained by insert bounds of secondary shelf.
  EXPECT_FALSE(drag_reinsert_bounds_in_secondary.Contains(point_in_shelf_view));
}

// Checks the rip an item off from left aligned shelf in secondary monitor.
TEST_F(ShelfViewTest, CheckRipOffFromLeftShelfAlignmentWithMultiMonitor) {
  UpdateDisplay("800x600,800x600");
  ASSERT_EQ(2U, Shell::GetAllRootWindows().size());

  Shelf* secondary_shelf = Shelf::ForWindow(Shell::GetAllRootWindows()[1]);

  secondary_shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  ASSERT_EQ(SHELF_ALIGNMENT_LEFT, secondary_shelf->alignment());

  ShelfView* shelf_view_for_secondary =
      secondary_shelf->GetShelfViewForTesting();

  ShelfViewTestAPI test_api_for_secondary_shelf_view(shelf_view_for_secondary);
  ShelfButton* button = test_api_for_secondary_shelf_view.GetButton(2);

  // Fetch the start point of dragging.
  gfx::Point start_point = button->GetBoundsInScreen().CenterPoint();
  ::wm::ConvertPointFromScreen(secondary_shelf->GetWindow(), &start_point);
  ui::test::EventGenerator generator(Shell::GetAllRootWindows()[1],
                                     start_point);

  // Rip off the browser item.
  generator.PressLeftButton();
  generator.MoveMouseTo(start_point.x() + 400, start_point.y());
  test_api_for_secondary_shelf_view.RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(test_api_for_secondary_shelf_view.IsRippedOffFromShelf());
}

// Checks various drag and drop operations from OverflowBubble to Shelf, and
// vice versa.
TEST_F(ShelfViewTest, CheckDragAndDropFromShelfToOtherShelf) {
  AddButtonsUntilOverflow();
  // Add one more button to prevent the overflow bubble to disappear upon
  // dragging an item out on windows (flakiness, see crbug.com/425097).
  AddAppShortcut();

  TestDraggingAnItemFromShelfToOtherShelf(false /* main_to_overflow */,
                                          false /* cancel */);
  TestDraggingAnItemFromShelfToOtherShelf(false /* main_to_overflow */,
                                          true /* cancel */);

  TestDraggingAnItemFromShelfToOtherShelf(true /* main_to_overflow */,
                                          false /* cancel */);
  TestDraggingAnItemFromShelfToOtherShelf(true /* main_to_overflow */,
                                          true /* cancel */);
}

// Checks creating app shortcut for an opened platform app in overflow bubble
// should be invisible to the shelf. See crbug.com/605793.
TEST_F(ShelfViewTest, CheckOverflowStatusPinOpenedAppToShelf) {
  AddButtonsUntilOverflow();

  // Add a running Platform app.
  ShelfID platform_app_id = AddApp();
  EXPECT_FALSE(GetButtonByID(platform_app_id)->visible());

  // Make the added running platform app to be an app shortcut.
  // This app shortcut should be a swapped view in overflow bubble, which is
  // invisible.
  SetShelfItemTypeToAppShortcut(platform_app_id);
  EXPECT_FALSE(GetButtonByID(platform_app_id)->visible());
}

// Verifies that Launcher_ButtonPressed_* UMA user actions are recorded when an
// item is selected.
TEST_F(ShelfViewTest,
       Launcher_ButtonPressedUserActionsRecordedWhenItemSelected) {
  base::UserActionTester user_action_tester;

  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->SetShelfItemDelegate(
      model_->items()[2].id,
      base::WrapUnique<ShelfItemSelectionTracker>(selection_tracker));

  SimulateClick(2);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("Launcher_ButtonPressed_Mouse"));
}

// Verifies that Launcher_*Task UMA user actions are recorded when an item is
// selected.
TEST_F(ShelfViewTest, Launcher_TaskUserActionsRecordedWhenItemSelected) {
  base::UserActionTester user_action_tester;

  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  selection_tracker->set_item_selected_action(SHELF_ACTION_NEW_WINDOW_CREATED);
  model_->SetShelfItemDelegate(
      model_->items()[2].id,
      base::WrapUnique<ShelfItemSelectionTracker>(selection_tracker));

  SimulateClick(2);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Launcher_LaunchTask"));
}

// Verifies that metrics are recorded when an item is minimized and subsequently
// activated.
TEST_F(ShelfViewTest,
       VerifyMetricsAreRecordedWhenAnItemIsMinimizedAndActivated) {
  base::HistogramTester histogram_tester;

  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->SetShelfItemDelegate(
      model_->items()[2].id,
      base::WrapUnique<ShelfItemSelectionTracker>(selection_tracker));

  selection_tracker->set_item_selected_action(SHELF_ACTION_WINDOW_MINIMIZED);
  SimulateClick(2);

  selection_tracker->set_item_selected_action(SHELF_ACTION_WINDOW_ACTIVATED);
  SimulateClick(2);

  histogram_tester.ExpectTotalCount(
      kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName, 1);
}

TEST_F(ShelfViewTest, TestHideOverflow) {
  // Use an event generator instead of SimulateClick because the overflow bubble
  // uses a Shell pre-target EventHandler to observe input events.
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Add one app (which is on the main shelf) and then add buttons until
  // overflow. Add two more apps (which are on the overflow shelf).
  ShelfID first_app_id = AddAppShortcut();
  ShelfID second_app_id = AddAppShortcut();
  AddButtonsUntilOverflow();
  ShelfID overflow_app_id1 = AddAppShortcut();
  ShelfID overflow_app_id2 = AddAppShortcut();

  // Verify that by pressing anywhere outside the shelf and overflow bubble, the
  // overflow bubble will close if it were open.
  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
  test_api_->ShowOverflowBubble();

  // Make sure the point we chose is not on the shelf or its overflow bubble.
  ASSERT_FALSE(
      shelf_view_->GetBoundsInScreen().Contains(generator->current_location()));
  ASSERT_FALSE(test_api_->overflow_bubble()
                   ->bubble_view()
                   ->shelf_view()
                   ->GetBoundsInScreen()
                   .Contains(generator->current_location()));
  generator->PressLeftButton();
  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
  generator->ReleaseLeftButton();

  // Verify that by clicking a app which is on the main shelf while the overflow
  // bubble is opened, the overflow bubble will close.
  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
  test_api_->ShowOverflowBubble();
  generator->set_current_location(GetButtonCenter(first_app_id));
  generator->ClickLeftButton();
  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());

  // Verify that by clicking a app which is on the overflow shelf, the overflow
  // bubble will close.
  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
  test_api_->ShowOverflowBubble();
  ShelfViewTestAPI test_api_for_overflow(
      test_api_->overflow_bubble()->bubble_view()->shelf_view());
  ShelfButton* button_on_overflow_shelf =
      test_api_for_overflow.GetButton(model_->ItemIndexByID(overflow_app_id2));
  generator->set_current_location(GetButtonCenter(button_on_overflow_shelf));
  generator->ClickLeftButton();
  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());

  // Verify that dragging apps on the main shelf does not close the overflow
  // bubble.
  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
  test_api_->ShowOverflowBubble();
  generator->set_current_location(GetButtonCenter(first_app_id));
  generator->DragMouseTo(GetButtonCenter(second_app_id));
  EXPECT_TRUE(test_api_->IsShowingOverflowBubble());
  test_api_->HideOverflowBubble();

  // Verify dragging apps on the overflow shelf does not close the overflow
  // bubble.
  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
  test_api_->ShowOverflowBubble();
  ShelfViewTestAPI test_api_for_overflow2(
      test_api_->overflow_bubble()->bubble_view()->shelf_view());
  button_on_overflow_shelf =
      test_api_for_overflow2.GetButton(model_->ItemIndexByID(overflow_app_id1));
  ShelfButton* button_on_overflow_shelf1 =
      test_api_for_overflow2.GetButton(model_->ItemIndexByID(overflow_app_id2));
  generator->set_current_location(GetButtonCenter(button_on_overflow_shelf));
  generator->DragMouseTo(GetButtonCenter(button_on_overflow_shelf1));
  EXPECT_TRUE(test_api_->IsShowingOverflowBubble());
}

TEST_F(ShelfViewTest, UnpinningCancelsOverflow) {
  // Add just enough items for overflow; one fewer would not require overflow.
  const ShelfID first_shelf_id = AddAppShortcut();
  AddButtonsUntilOverflow();
  test_api_->ShowOverflowBubble();
  EXPECT_TRUE(test_api_->IsOverflowButtonVisible());
  EXPECT_TRUE(test_api_->IsShowingOverflowBubble());

  // Unpinning an item should hide the overflow button and close the bubble.
  model_->UnpinAppWithID(first_shelf_id.app_id);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Verify the animations of the shelf items are as long as expected.
TEST_F(ShelfViewTest, TestShelfItemsAnimations) {
  TestShelfObserver observer(shelf_view_->shelf());
  ui::test::EventGenerator* generator = GetEventGenerator();
  ShelfID first_app_id = AddAppShortcut();
  ShelfID second_app_id = AddAppShortcut();

  // Set the animation duration for shelf items.
  const int animation_duration = 100;
  test_api_->SetAnimationDuration(animation_duration);

  // The shelf items should animate if they are moved within the shelf, either
  // by swapping or if the items need to be rearranged due to an item getting
  // ripped off.
  generator->set_current_location(GetButtonCenter(first_app_id));
  generator->DragMouseTo(GetButtonCenter(second_app_id));
  generator->DragMouseBy(0, 50);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(animation_duration, observer.icon_positions_animation_duration());

  // The shelf items should not animate when the whole shelf and its contents
  // have to move.
  observer.Reset();
  shelf_view_->shelf()->SetAlignment(SHELF_ALIGNMENT_LEFT);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, observer.icon_positions_animation_duration());

  // The shelf items should animate if we are entering or exiting tablet mode,
  // and the shelf alignment is bottom aligned.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  const int64_t id = GetPrimaryDisplay().id();
  shelf_view_->shelf()->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
  SetShelfAlignmentPref(prefs, id, SHELF_ALIGNMENT_BOTTOM);
  observer.Reset();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(animation_duration, observer.icon_positions_animation_duration());

  observer.Reset();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(animation_duration, observer.icon_positions_animation_duration());

  // The shelf items should not animate if we are entering or exiting tablet
  // mode, and the shelf alignment is not bottom aligned.
  shelf_view_->shelf()->SetAlignment(SHELF_ALIGNMENT_LEFT);
  SetShelfAlignmentPref(prefs, id, SHELF_ALIGNMENT_LEFT);
  observer.Reset();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, observer.icon_positions_animation_duration());

  observer.Reset();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, observer.icon_positions_animation_duration());
}

// Tests that the blank shelf view area shows a context menu on right click.
TEST_F(ShelfViewTest, ShelfViewShowsContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(shelf_view_->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();
  generator->ReleaseRightButton();

  EXPECT_TRUE(test_api_->CloseMenu());
}

TEST_F(ShelfViewTest, TabletModeStartAndEndClosesContextMenu) {
  // Show a context menu on the shelf
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(shelf_view_->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();

  // Start tablet mode, which should close the menu.
  shelf_view_->OnTabletModeStarted();

  // Attempt to close the menu, which should already be closed.
  EXPECT_FALSE(test_api_->CloseMenu());

  // Show another context menu on the shelf.
  generator->MoveMouseTo(shelf_view_->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();

  // End tablet mode, which should close the menu.
  shelf_view_->OnTabletModeEnded();

  // Attempt to close the menu, which should already be closed.
  EXPECT_FALSE(test_api_->CloseMenu());
}

// Tests that the back button does not show a context menu.
TEST_F(ShelfViewTest, NoContextMenuOnBackButton) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Enable tablet mode to show the back button. Wait for tablet mode animations
  // to finish in order for the BackButton to move out from under the
  // AppListButton.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  test_api_->RunMessageLoopUntilAnimationsDone();

  views::View* back_button = shelf_view_->GetBackButton();
  generator->MoveMouseTo(back_button->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();

  EXPECT_FALSE(test_api_->CloseMenu());
}

// Tests that the overflow button does not show a context menu.
TEST_F(ShelfViewTest, NoContextMenuOnOverflowButton) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  AddButtonsUntilOverflow();
  views::View* overflow_button = test_api_->overflow_button();

  generator->MoveMouseTo(overflow_button->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();

  EXPECT_FALSE(test_api_->CloseMenu());
}

// Tests that ShelfWindowWatcher buttons show a context menu on right click.
TEST_F(ShelfViewTest, ShelfWindowWatcherButtonShowsContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  ShelfID shelf_id("123");
  window->SetProperty(kShelfIDKey, new std::string(shelf_id.Serialize()));
  window->SetProperty(kShelfItemTypeKey, static_cast<int32_t>(TYPE_DIALOG));
  ShelfButton* button = GetButtonByID(shelf_id);
  ASSERT_TRUE(button);
  generator->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();
  EXPECT_TRUE(test_api_->CloseMenu());
}

// Tests that the drag view is set on left click and not set on right click.
TEST_F(ShelfViewTest, ShelfDragViewAndContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  ShelfID shelf_id("123");
  window->SetProperty(kShelfIDKey, new std::string(shelf_id.Serialize()));
  window->SetProperty(kShelfItemTypeKey, static_cast<int32_t>(TYPE_DIALOG));
  ShelfButton* button = GetButtonByID(shelf_id);
  ASSERT_TRUE(button);

  // Context menu is shown on right button press and no drag view is set.
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  generator->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();
  EXPECT_TRUE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->drag_view());

  // Press left button. Menu should close.
  generator->PressLeftButton();
  generator->ReleaseLeftButton();
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  // Press left button. Drag view is set to |button|.
  generator->PressLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(shelf_view_->drag_view(), button);
  generator->ReleaseLeftButton();
  EXPECT_FALSE(shelf_view_->drag_view());
}

struct TouchableAppContextMenuTestParams {
  TouchableAppContextMenuTestParams(bool enable_touchable_app_context_menu,
                                    bool context_menu)
      : enable_touchable_app_context_menu(enable_touchable_app_context_menu),
        context_menu(context_menu) {}
  // Whether to enable the touchable app context menu feature.
  bool enable_touchable_app_context_menu;
  // Whether the menu is shown as an application or context menu.
  bool context_menu;
};

class ShelfViewTouchableContextMenuTest
    : public ShelfViewTest,
      public testing::WithParamInterface<TouchableAppContextMenuTestParams> {
 public:
  ShelfViewTouchableContextMenuTest() = default;
  ~ShelfViewTouchableContextMenuTest() override = default;

  void SetUp() override {
    // If the test is parameterized, respect the parameter. Otherwise enable
    // touchable app context menus by default.
    const bool enable_touchable_app_context_menu =
        testing::UnitTest::GetInstance()->current_test_info()->value_param()
            ? GetParam().enable_touchable_app_context_menu
            : true;
    std::vector<base::Feature> enabled_features = {
        features::kNotificationIndicator};
    if (enable_touchable_app_context_menu)
      enabled_features.push_back(features::kTouchableAppContextMenu);
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
    ShelfViewTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ShelfViewTouchableContextMenuTest);
};

INSTANTIATE_TEST_CASE_P(
    TouchableDisabledAppMenu,
    ShelfViewTouchableContextMenuTest,
    ::testing::Values(TouchableAppContextMenuTestParams(false, false)));

INSTANTIATE_TEST_CASE_P(
    TouchableEnabledAppMenu,
    ShelfViewTouchableContextMenuTest,
    ::testing::Values(TouchableAppContextMenuTestParams(true, false)));

INSTANTIATE_TEST_CASE_P(
    TouchableDisabledContextMenu,
    ShelfViewTouchableContextMenuTest,
    ::testing::Values(TouchableAppContextMenuTestParams(false, true)));

INSTANTIATE_TEST_CASE_P(
    TouchableEnabledContextMenu,
    ShelfViewTouchableContextMenuTest,
    ::testing::Values(TouchableAppContextMenuTestParams(true, true)));

// Tests that menu anchor points are aligned with the shelf button bounds.
TEST_P(ShelfViewTouchableContextMenuTest, ShelfViewMenuAnchorPoint) {
  const ShelfButton* shelf_button = GetButtonByID(AddApp());
  const bool context_menu = GetParam().context_menu;
  EXPECT_EQ(ash::ShelfAlignment::SHELF_ALIGNMENT_BOTTOM,
            GetPrimaryShelf()->alignment());

  // Test for bottom shelf.
  EXPECT_EQ(
      shelf_button->GetBoundsInScreen().y(),
      test_api_->GetMenuAnchorRect(*shelf_button, gfx::Point(), context_menu)
          .y());

  // Test for left shelf.
  GetPrimaryShelf()->SetAlignment(ash::ShelfAlignment::SHELF_ALIGNMENT_LEFT);

  int expected_x = shelf_button->GetBoundsInScreen().x();
  // Left shelf context menus when TouchableAppContextMenu is disabled anchor
  // off of the right edge of the shelf.
  if (context_menu && !features::IsTouchableAppContextMenuEnabled())
    expected_x = shelf_button->GetBoundsInScreen().right();
  EXPECT_EQ(
      expected_x,
      test_api_->GetMenuAnchorRect(*shelf_button, gfx::Point(), context_menu)
          .x());

  // Test for right shelf.
  GetPrimaryShelf()->SetAlignment(ash::ShelfAlignment::SHELF_ALIGNMENT_RIGHT);

  EXPECT_EQ(
      shelf_button->GetBoundsInScreen().x(),
      test_api_->GetMenuAnchorRect(*shelf_button, gfx::Point(), context_menu)
          .x());
}

// Tests that the app list button does not show a context menu on right click
// when touchable app context menus are enabled.
TEST_F(ShelfViewTouchableContextMenuTest, AppListButtonDoesNotShowContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  const AppListButton* app_list_button = shelf_view_->GetAppListButton();
  generator->MoveMouseTo(app_list_button->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();
  EXPECT_FALSE(test_api_->CloseMenu());
}

// Tests that an item has a notification indicator when it recieves a
// notification.
TEST_F(ShelfViewTouchableContextMenuTest, AddedItemHasNotificationIndicator) {
  const ShelfID id_0 = AddApp();
  const std::string notification_id_0("notification_id_0");
  const ShelfButton* button_0 = GetButtonByID(id_0);

  EXPECT_FALSE(GetItemByID(id_0).has_notification);
  EXPECT_FALSE(button_0->state() & ShelfButton::STATE_NOTIFICATION);

  // Post a test notification after the item was added.
  model_->AddNotificationRecord(id_0.app_id, notification_id_0);

  EXPECT_TRUE(GetItemByID(id_0).has_notification);
  EXPECT_TRUE(button_0->state() & ShelfButton::STATE_NOTIFICATION);

  // Post another notification for a non existing item.
  const std::string next_app_id(GetNextAppId());
  const std::string notification_id_1("notification_id_1");
  model_->AddNotificationRecord(next_app_id, notification_id_1);

  // Add an item with matching app id.
  const ShelfID id_1 = AddApp();

  // Ensure that the app id assigned to |id_1| is the same as |next_app_id|.
  EXPECT_EQ(next_app_id, id_1.app_id);
  const ShelfButton* button_1 = GetButtonByID(id_1);
  EXPECT_TRUE(GetItemByID(id_1).has_notification);
  EXPECT_TRUE(button_1->state() & ShelfButton::STATE_NOTIFICATION);

  // Remove all notifications.
  model_->RemoveNotificationRecord(notification_id_0);
  model_->RemoveNotificationRecord(notification_id_1);

  EXPECT_FALSE(GetItemByID(id_0).has_notification);
  EXPECT_FALSE(button_0->state() & ShelfButton::STATE_NOTIFICATION);
  EXPECT_FALSE(GetItemByID(id_1).has_notification);
  EXPECT_FALSE(button_1->state() & ShelfButton::STATE_NOTIFICATION);
}

// Tests that the notification indicator is active until all notifications have
// been removed.
TEST_F(ShelfViewTouchableContextMenuTest,
       NotificationIndicatorStaysActiveUntilNotificationsAreGone) {
  const ShelfID app = AddApp();
  const ShelfButton* button = GetButtonByID(app);

  // Add two notifications for the same app.
  const std::string notification_id_0("notification_id_0");
  model_->AddNotificationRecord(app.app_id, notification_id_0);
  const std::string notification_id_1("notification_id_1");
  model_->AddNotificationRecord(app.app_id, notification_id_1);

  EXPECT_TRUE(GetItemByID(app).has_notification);
  EXPECT_TRUE(button->state() & ShelfButton::STATE_NOTIFICATION);

  // Remove one notification, indicator should stay active.
  model_->RemoveNotificationRecord(notification_id_0);

  EXPECT_TRUE(GetItemByID(app).has_notification);
  EXPECT_TRUE(button->state() & ShelfButton::STATE_NOTIFICATION);

  // Remove the last notification, indicator should not be active.
  model_->RemoveNotificationRecord(notification_id_1);

  EXPECT_FALSE(GetItemByID(app).has_notification);
  EXPECT_FALSE(button->state() & ShelfButton::STATE_NOTIFICATION);
}

class ShelfViewVisibleBoundsTest : public ShelfViewTest,
                                   public testing::WithParamInterface<bool> {
 public:
  ShelfViewVisibleBoundsTest() : scoped_locale_(GetParam() ? "he" : "") {}

  void CheckAllItemsAreInBounds() {
    gfx::Rect visible_bounds = shelf_view_->GetVisibleItemsBoundsInScreen();
    gfx::Rect shelf_bounds = shelf_view_->GetBoundsInScreen();
    EXPECT_TRUE(shelf_bounds.Contains(visible_bounds));
    for (int i = 0; i < test_api_->GetButtonCount(); ++i)
      if (ShelfButton* button = test_api_->GetButton(i)) {
        if (button->visible())
          EXPECT_TRUE(visible_bounds.Contains(button->GetBoundsInScreen()));
      }
    CheckAppListButtonIsInBounds();
  }

  void CheckAppListButtonIsInBounds() {
    gfx::Rect visible_bounds = shelf_view_->GetVisibleItemsBoundsInScreen();
    gfx::Rect app_list_button_bounds =
        shelf_view_->GetAppListButton()->GetBoundsInScreen();
    EXPECT_TRUE(visible_bounds.Contains(app_list_button_bounds));
  }

 private:
  // Restores locale to the default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;

  DISALLOW_COPY_AND_ASSIGN(ShelfViewVisibleBoundsTest);
};

TEST_P(ShelfViewVisibleBoundsTest, ItemsAreInBounds) {
  // Adding elements leaving some empty space.
  for (int i = 0; i < 3; i++) {
    AddAppShortcut();
  }
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
  CheckAllItemsAreInBounds();
  // Same for overflow case.
  while (!test_api_->IsOverflowButtonVisible()) {
    AddAppShortcut();
  }
  test_api_->RunMessageLoopUntilAnimationsDone();
  CheckAllItemsAreInBounds();
}

INSTANTIATE_TEST_CASE_P(LtrRtl, ShelfViewTextDirectionTest, testing::Bool());
INSTANTIATE_TEST_CASE_P(VisibleBounds,
                        ShelfViewVisibleBoundsTest,
                        testing::Bool());

namespace {

// An InkDrop implementation that wraps another InkDrop instance to keep track
// of state changes requested on it. Note that this will only track transitions
// routed through AnimateToState() and not the ones performed directly on the
// ripple inside the contained |ink_drop|.
class InkDropSpy : public views::InkDrop {
 public:
  explicit InkDropSpy(std::unique_ptr<views::InkDrop> ink_drop)
      : ink_drop_(std::move(ink_drop)) {}
  ~InkDropSpy() override = default;

  std::vector<views::InkDropState> GetAndResetRequestedStates() {
    std::vector<views::InkDropState> requested_states;
    requested_states.swap(requested_states_);
    return requested_states;
  }

  // views::InkDrop:
  void HostSizeChanged(const gfx::Size& new_size) override {
    ink_drop_->HostSizeChanged(new_size);
  }
  views::InkDropState GetTargetInkDropState() const override {
    return ink_drop_->GetTargetInkDropState();
  }
  void AnimateToState(views::InkDropState ink_drop_state) override {
    requested_states_.push_back(ink_drop_state);
    ink_drop_->AnimateToState(ink_drop_state);
  }

  void SetHoverHighlightFadeDurationMs(int duration_ms) override {
    ink_drop_->SetHoverHighlightFadeDurationMs(duration_ms);
  }

  void UseDefaultHoverHighlightFadeDuration() override {
    ink_drop_->UseDefaultHoverHighlightFadeDuration();
  }

  void SnapToActivated() override { ink_drop_->SnapToActivated(); }
  void SnapToHidden() override { ink_drop_->SnapToHidden(); }
  void SetHovered(bool is_hovered) override {
    ink_drop_->SetHovered(is_hovered);
  }
  void SetFocused(bool is_focused) override {
    ink_drop_->SetFocused(is_focused);
  }

  bool IsHighlightFadingInOrVisible() const override {
    return ink_drop_->IsHighlightFadingInOrVisible();
  }

  void SetShowHighlightOnHover(bool show_highlight_on_hover) override {
    ink_drop_->SetShowHighlightOnHover(show_highlight_on_hover);
  }

  void SetShowHighlightOnFocus(bool show_highlight_on_focus) override {
    ink_drop_->SetShowHighlightOnFocus(show_highlight_on_focus);
  }

  std::unique_ptr<views::InkDrop> ink_drop_;
  std::vector<views::InkDropState> requested_states_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropSpy);
};

// A ShelfItemDelegate that returns a menu for the shelf item.
class ListMenuShelfItemDelegate : public ShelfItemDelegate {
 public:
  ListMenuShelfItemDelegate() : ShelfItemDelegate(ShelfID()) {}
  ~ListMenuShelfItemDelegate() override = default;

 private:
  // ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback) override {
    // Two items are needed to show a menu; the data in the items is not tested.
    std::vector<mojom::MenuItemPtr> items;
    items.push_back(mojom::MenuItem::New());
    items.push_back(mojom::MenuItem::New());
    std::move(callback).Run(SHELF_ACTION_NONE, std::move(items));
  }
  void ExecuteCommand(bool, int64_t, int32_t, int64_t) override {}
  void Close() override {}

  DISALLOW_COPY_AND_ASSIGN(ListMenuShelfItemDelegate);
};

}  // namespace

// Test fixture for testing material design ink drop ripples on shelf.
class ShelfViewInkDropTest : public ShelfViewTest {
 public:
  ShelfViewInkDropTest() = default;
  ~ShelfViewInkDropTest() override = default;

  void SetUp() override {
    ash_test_helper()->set_test_shell_delegate(new TestShellDelegate());
    ShelfViewTest::SetUp();
  }

 protected:
  void InitAppListButtonInkDrop() {
    app_list_button_ = shelf_view_->GetAppListButton();

    auto app_list_button_ink_drop =
        std::make_unique<InkDropSpy>(std::make_unique<views::InkDropImpl>(
            app_list_button_, app_list_button_->size()));
    app_list_button_ink_drop_ = app_list_button_ink_drop.get();
    views::test::InkDropHostViewTestApi(app_list_button_)
        .SetInkDrop(std::move(app_list_button_ink_drop), false);
  }

  void InitBrowserButtonInkDrop() {
    browser_button_ = test_api_->GetButton(2);

    auto browser_button_ink_drop =
        std::make_unique<InkDropSpy>(std::make_unique<views::InkDropImpl>(
            browser_button_, browser_button_->size()));
    browser_button_ink_drop_ = browser_button_ink_drop.get();
    views::test::InkDropHostViewTestApi(browser_button_)
        .SetInkDrop(std::move(browser_button_ink_drop));
  }

  AppListButton* app_list_button_ = nullptr;
  InkDropSpy* app_list_button_ink_drop_ = nullptr;
  ShelfButton* browser_button_ = nullptr;
  InkDropSpy* browser_button_ink_drop_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfViewInkDropTest);
};

// Tests that changing visibility of the app list transitions app list button's
// ink drop states correctly.
TEST_F(ShelfViewInkDropTest, AppListButtonWhenVisibilityChanges) {
  InitAppListButtonInkDrop();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  GetAppListTestHelper()->DismissAndRunLoop();

  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));
}

// Tests that when the app list is hidden, mouse press on the app list button,
// which shows the app list, transitions ink drop states correctly. Also, tests
// that mouse drag and mouse release does not affect the ink drop state.
TEST_F(ShelfViewInkDropTest, AppListButtonMouseEventsWhenHidden) {
  InitAppListButtonInkDrop();

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(app_list_button_->GetBoundsInScreen().CenterPoint());

  // Mouse press on the button, which shows the app list, should end up in the
  // activated state.
  generator->PressLeftButton();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING,
                          views::InkDropState::ACTIVATED));

  // Dragging mouse out and back and releasing the button should not change the
  // ink drop state.
  generator->MoveMouseBy(app_list_button_->width(), 0);
  generator->MoveMouseBy(-app_list_button_->width(), 0);
  generator->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());
}

// Tests that when the app list is visible, mouse press on the app list button,
// which dismisses the app list, transitions ink drop states correctly. Also,
// tests that mouse drag and mouse release does not affect the ink drop state.
TEST_F(ShelfViewInkDropTest, AppListButtonMouseEventsWhenVisible) {
  InitAppListButtonInkDrop();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  // Mouse press on the button, which dismisses the app list, should end up in
  // the hidden state.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(app_list_button_->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));

  // Dragging mouse out and back and releasing the button should not change the
  // ink drop state.
  generator->MoveMouseBy(app_list_button_->width(), 0);
  generator->MoveMouseBy(-app_list_button_->width(), 0);
  generator->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());
}

// Tests that when the app list is hidden, tapping on the app list button
// transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, AppListButtonGestureTapWhenHidden) {
  InitAppListButtonInkDrop();

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(app_list_button_->GetBoundsInScreen().CenterPoint());

  // Touch press on the button should end up in the pending state.
  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  // Touch release on the button, which shows the app list, should end up in the
  // activated state.
  generator->ReleaseTouch();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));
}

// Tests that when the app list is visible, tapping on the app list button
// transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, AppListButtonGestureTapWhenVisible) {
  InitAppListButtonInkDrop();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  // Touch press and release on the button, which dismisses the app list, should
  // end up in the hidden state.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(app_list_button_->GetBoundsInScreen().CenterPoint());
  generator->PressTouch();
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));
}

// Tests that when the app list is hidden, tapping down on the app list button
// and dragging the touch point transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, AppListButtonGestureTapDragWhenHidden) {
  InitAppListButtonInkDrop();

  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Point touch_location =
      app_list_button_->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(touch_location);

  // Touch press on the button should end up in the pending state.
  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  // Dragging the touch point should hide the pending ink drop.
  touch_location.Offset(app_list_button_->width(), 0);
  generator->MoveTouch(touch_location);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  // Touch release should not change the ink drop state.
  generator->ReleaseTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());
}

// Tests that when the app list is visible, tapping down on the app list button
// and dragging the touch point transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, AppListButtonGestureTapDragWhenVisible) {
  InitAppListButtonInkDrop();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  // Touch press on the button, dragging the touch point, and releasing, which
  // dismisses the app list, should end up in the hidden state.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(app_list_button_->GetBoundsInScreen().CenterPoint());
  generator->PressMoveAndReleaseTouchBy(app_list_button_->width(), 0);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));
}

// Tests that clicking on a shelf item that does not show a menu transitions ink
// drop states correctly.
TEST_F(ShelfViewInkDropTest, ShelfButtonWithoutMenuPressRelease) {
  InitBrowserButtonInkDrop();

  views::Button* button = browser_button_;
  gfx::Point mouse_location = button->GetLocalBounds().CenterPoint();

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, mouse_location,
                               mouse_location, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_TRIGGERED));
}

// Tests that dragging outside of a shelf item transitions ink drop states
// correctly.
TEST_F(ShelfViewInkDropTest, ShelfButtonWithoutMenuPressDragReleaseOutside) {
  InitBrowserButtonInkDrop();

  views::Button* button = browser_button_;
  gfx::Point mouse_location = button->GetLocalBounds().CenterPoint();

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  mouse_location.Offset(test_api_->GetMinimumDragDistance() / 2, 0);
  ui::MouseEvent drag_event_small(ui::ET_MOUSE_DRAGGED, mouse_location,
                                  mouse_location, ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event_small);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  mouse_location.Offset(test_api_->GetMinimumDragDistance(), 0);
  ui::MouseEvent drag_event_large(ui::ET_MOUSE_DRAGGED, mouse_location,
                                  mouse_location, ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event_large);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, mouse_location,
                               mouse_location, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());
}

// Tests that dragging outside of a shelf item and back transitions ink drop
// states correctly.
TEST_F(ShelfViewInkDropTest, ShelfButtonWithoutMenuPressDragReleaseInside) {
  InitBrowserButtonInkDrop();

  views::Button* button = browser_button_;
  gfx::Point mouse_location = button->GetLocalBounds().CenterPoint();

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  mouse_location.Offset(test_api_->GetMinimumDragDistance() * 2, 0);
  ui::MouseEvent drag_event_outside(ui::ET_MOUSE_DRAGGED, mouse_location,
                                    mouse_location, ui::EventTimeForNow(),
                                    ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event_outside);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  mouse_location.Offset(-test_api_->GetMinimumDragDistance() * 2, 0);
  ui::MouseEvent drag_event_inside(ui::ET_MOUSE_DRAGGED, mouse_location,
                                   mouse_location, ui::EventTimeForNow(),
                                   ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event_inside);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, mouse_location,
                               mouse_location, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());
}

// Tests that clicking on a shelf item that shows an app list menu transitions
// ink drop state correctly.
TEST_F(ShelfViewInkDropTest, ShelfButtonWithMenuPressRelease) {
  InitBrowserButtonInkDrop();

  // Set a delegate for the shelf item that returns an app list menu.
  model_->SetShelfItemDelegate(model_->items()[2].id,
                               std::make_unique<ListMenuShelfItemDelegate>());

  views::Button* button = browser_button_;
  gfx::Point mouse_location = button->GetLocalBounds().CenterPoint();

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  // Mouse release will spawn a menu which we will then close.
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, mouse_location,
                               mouse_location, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_TRUE(test_api_->CloseMenu());

  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED,
                          views::InkDropState::DEACTIVATED));
}

// Test fixture for testing material design ink drop on overflow button.
class OverflowButtonInkDropTest : public ShelfViewInkDropTest {
 public:
  OverflowButtonInkDropTest() = default;
  ~OverflowButtonInkDropTest() override = default;

  void SetUp() override {
    ShelfViewInkDropTest::SetUp();

    overflow_button_ = test_api_->overflow_button();

    auto overflow_button_ink_drop =
        std::make_unique<InkDropSpy>(std::make_unique<views::InkDropImpl>(
            overflow_button_, overflow_button_->size()));
    overflow_button_ink_drop_ = overflow_button_ink_drop.get();
    views::test::InkDropHostViewTestApi(overflow_button_)
        .SetInkDrop(std::move(overflow_button_ink_drop));

    AddButtonsUntilOverflow();
    EXPECT_TRUE(test_api_->IsOverflowButtonVisible());
    EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
  }

 protected:
  gfx::Point GetScreenPointInsideOverflowButton() const {
    return overflow_button_->GetBoundsInScreen().CenterPoint();
  }

  gfx::Point GetScreenPointOutsideOverflowButton() const {
    gfx::Point point = GetScreenPointInsideOverflowButton();
    point.Offset(overflow_button_->width(), 0);
    return point;
  }

  OverflowButton* overflow_button_ = nullptr;
  InkDropSpy* overflow_button_ink_drop_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(OverflowButtonInkDropTest);
};

// Tests ink drop state transitions for the overflow button when the overflow
// bubble is shown or hidden.
TEST_F(OverflowButtonInkDropTest, OnOverflowBubbleShowHide) {
  test_api_->ShowOverflowBubble();
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  test_api_->HideOverflowBubble();
  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));
}

// Tests ink drop state transitions for the overflow button when the user
// clicks on it.
TEST_F(OverflowButtonInkDropTest, MouseActivate) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Point mouse_location = GetScreenPointInsideOverflowButton();
  generator->MoveMouseTo(mouse_location);

  generator->PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when the user
// presses left mouse button on it and drags it out of the button bounds.
TEST_F(OverflowButtonInkDropTest, MouseDragOut) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetScreenPointInsideOverflowButton());

  generator->PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator->MoveMouseTo(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  generator->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when the user
// presses left mouse button on it and drags it out of the button bounds and
// back.
TEST_F(OverflowButtonInkDropTest, MouseDragOutAndBack) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetScreenPointInsideOverflowButton());

  generator->PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator->MoveMouseTo(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  generator->MoveMouseTo(GetScreenPointInsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when the user
// right clicks on the button to show the context menu.
TEST_F(OverflowButtonInkDropTest, MouseContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetScreenPointInsideOverflowButton());

  generator->PressRightButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->ReleaseRightButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when the user taps
// on it.
TEST_F(OverflowButtonInkDropTest, TouchActivate) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(GetScreenPointInsideOverflowButton());

  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator->ReleaseTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when the user taps
// down on it and drags it out of the button bounds.
TEST_F(OverflowButtonInkDropTest, TouchDragOut) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(GetScreenPointInsideOverflowButton());

  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator->MoveTouch(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  generator->ReleaseTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when the user taps
// down on it and drags it out of the button bounds and back.
TEST_F(OverflowButtonInkDropTest, TouchDragOutAndBack) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(GetScreenPointInsideOverflowButton());

  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator->MoveTouch(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  generator->MoveTouch(GetScreenPointInsideOverflowButton());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->ReleaseTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Test fixture to run overflow button tests for LTR and RTL directions.
class OverflowButtonTextDirectionTest
    : public OverflowButtonInkDropTest,
      public testing::WithParamInterface<bool> {
 public:
  OverflowButtonTextDirectionTest() : scoped_locale_(GetParam() ? "he" : "") {}
  ~OverflowButtonTextDirectionTest() override = default;

  void SetUp() override {
    OverflowButtonInkDropTest::SetUp();
  }

 private:
  // Restores locale to the default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;

  DISALLOW_COPY_AND_ASSIGN(OverflowButtonTextDirectionTest);
};

INSTANTIATE_TEST_CASE_P(
    /* prefix intentionally left blank due to only one parameterization */,
    OverflowButtonTextDirectionTest,
    testing::Bool());

// Test fixture for testing material design ink drop on overflow button when
// it is active.
class OverflowButtonActiveInkDropTest : public OverflowButtonInkDropTest {
 public:
  OverflowButtonActiveInkDropTest() = default;
  ~OverflowButtonActiveInkDropTest() override = default;

  void SetUp() override {
    OverflowButtonInkDropTest::SetUp();

    test_api_->ShowOverflowBubble();
    ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
    EXPECT_EQ(views::InkDropState::ACTIVATED,
              overflow_button_ink_drop_->GetTargetInkDropState());
    EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
                ElementsAre(views::InkDropState::ACTIVATED));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverflowButtonActiveInkDropTest);
};

// Tests ink drop state transitions for the overflow button when it is active
// and the user clicks on it.
TEST_F(OverflowButtonActiveInkDropTest, MouseDeactivate) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetScreenPointInsideOverflowButton());

  generator->PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when it is active
// and the user presses left mouse button on it and drags it out of the button
// bounds.
TEST_F(OverflowButtonActiveInkDropTest, MouseDragOut) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetScreenPointInsideOverflowButton());

  generator->PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->MoveMouseTo(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when it is active
// and the user presses left mouse button on it and drags it out of the button
// bounds and back.
TEST_F(OverflowButtonActiveInkDropTest, MouseDragOutAndBack) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetScreenPointInsideOverflowButton());

  generator->PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->MoveMouseTo(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->MoveMouseTo(GetScreenPointInsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when it is active
// and the user taps on it.
TEST_F(OverflowButtonActiveInkDropTest, TouchDeactivate) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(GetScreenPointInsideOverflowButton());

  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->ReleaseTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when it is active
// and the user taps down on it and drags it out of the button bounds.
TEST_F(OverflowButtonActiveInkDropTest, TouchDragOut) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(GetScreenPointInsideOverflowButton());

  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->MoveTouch(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->ReleaseTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when it is active
// and the user taps down on it and drags it out of the button bounds and
// back.
TEST_F(OverflowButtonActiveInkDropTest, TouchDragOutAndBack) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(GetScreenPointInsideOverflowButton());

  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->MoveTouch(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->MoveTouch(GetScreenPointInsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator->ReleaseTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

class ShelfViewFocusTest : public ShelfViewTest {
 public:
  ShelfViewFocusTest() = default;
  ~ShelfViewFocusTest() override = default;

  // AshTestBase:
  void SetUp() override {
    ShelfViewTest::SetUp();

    // Add two app shortcuts for testing.
    AddAppShortcut();
    AddAppShortcut();

    // Focus the shelf.
    Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
    Shell::Get()->focus_cycler()->FocusWidget(shelf->shelf_widget());
  }

  void DoTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EventFlags::EF_NONE);
  }

  void DoShiftTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB,
                       ui::EventFlags::EF_SHIFT_DOWN);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfViewFocusTest);
};

// Tests that the number of buttons is as expected and the shelf's widget
// intially has focus.
TEST_F(ShelfViewFocusTest, Basic) {
  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());

  // There are five buttons. The back button and launcher are always there, the
  // browser shortcut is added in ShelfViewTest and the two test apps added in
  // ShelfViewFocusTest.
  EXPECT_EQ(5, test_api_->GetButtonCount());
  EXPECT_TRUE(shelf_view_->shelf_widget()->IsActive());

  // The item at index 1 instead of index 0 is focused initially because index 0
  // is the back button which is only visible in tablet mode.
  EXPECT_TRUE(test_api_->GetViewAt(1)->HasFocus());
}

// Tests that the expected views have focus when cycling through shelf items
// with tab.
TEST_F(ShelfViewFocusTest, ForwardCycling) {
  // Pressing tab once should advance focus to the next element.
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(2)->HasFocus());

  DoTab();
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(4)->HasFocus());

  // The last element is currently focused so pressing tab once should advance
  // focus to the first element.
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(1)->HasFocus());
}

// Tests that the expected views have focus when cycling backwards through shelf
// items with shift tab.
TEST_F(ShelfViewFocusTest, BackwardCycling) {
  // The first element is currently focused so pressing shift tab once should
  // advance focus to the last element.
  DoShiftTab();
  EXPECT_TRUE(test_api_->GetViewAt(4)->HasFocus());

  // Pressing shift tab once should advance focus to the previous element.
  DoShiftTab();
  EXPECT_TRUE(test_api_->GetViewAt(3)->HasFocus());
}

// Verify that the overflow bubble does not activate when it is opened.
TEST_F(ShelfViewFocusTest, OverflowNotActivatedWhenOpened) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  ::wm::ActivateWindow(window.get());

  AddButtonsUntilOverflow();
  test_api_->ShowOverflowBubble();
  EXPECT_TRUE(::wm::IsActiveWindow(window.get()));
}

class ShelfViewOverflowFocusTest : public ShelfViewFocusTest {
 public:
  ShelfViewOverflowFocusTest() = default;
  ~ShelfViewOverflowFocusTest() override = default;

  // AshTestBase:
  void SetUp() override {
    ShelfViewFocusTest::SetUp();

    // Add app shortcuts until the overflow button is visible. At this point
    // there will be two items on the overflow shelf.
    AddButtonsUntilOverflow();

    // Add two more shortcuts for a total of four items on the overflow shelf.
    AddAppShortcut();
    AddAppShortcut();
    items_ = test_api_->GetButtonCount();
    last_item_on_main_shelf_index_ = shelf_view_->last_visible_index();
  }

  // Opens the overflow bubble. Focuses the main shelf for testing purposes.
  void OpenOverflow() {
    test_api_->ShowOverflowBubble();
    overflow_shelf_test_api_ = std::make_unique<ShelfViewTestAPI>(
        shelf_view_->overflow_bubble()->bubble_view()->shelf_view());

    Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
    Shell::Get()->focus_cycler()->FocusWidget(shelf->shelf_widget());
  }

 protected:
  int items_ = 0;
  int last_item_on_main_shelf_index_ = 0;
  std::unique_ptr<ShelfViewTestAPI> overflow_shelf_test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfViewOverflowFocusTest);
};

// Tests that the overflow button is visible and that not all the items are
// visible on the main shelf.
TEST_F(ShelfViewOverflowFocusTest, Basic) {
  EXPECT_TRUE(test_api_->IsOverflowButtonVisible());
  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());

  EXPECT_EQ(last_item_on_main_shelf_index_, items_ - 5);
  EXPECT_TRUE(shelf_view_->shelf_widget()->IsActive());
  EXPECT_TRUE(test_api_->GetViewAt(1)->HasFocus());
}

TEST_F(ShelfViewOverflowFocusTest, OpenOverflow) {
  OpenOverflow();
  ASSERT_TRUE(overflow_shelf_test_api_);
  EXPECT_TRUE(test_api_->IsShowingOverflowBubble());
  EXPECT_TRUE(test_api_->GetViewAt(1)->HasFocus());
}

// Tests that when cycling through the items with tab, the items in the overflow
// shelf are ignored because it is not visible.
TEST_F(ShelfViewOverflowFocusTest, ForwardCycling) {
  // Focus the last visible item on the shelf.
  shelf_view_->shelf_widget()->GetFocusManager()->SetFocusedView(
      test_api_->GetViewAt(last_item_on_main_shelf_index_));
  EXPECT_TRUE(test_api_->GetViewAt(last_item_on_main_shelf_index_)->HasFocus());

  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(1)->HasFocus());
}

// Tests that when cycling through the items with shift tab, the items in the
// overflow shelf are ignored because it is not visible.
TEST_F(ShelfViewOverflowFocusTest, BackwardCycling) {
  DoShiftTab();
  EXPECT_TRUE(test_api_->GetViewAt(last_item_on_main_shelf_index_)->HasFocus());
}

// Tests that cycling through elements with tab works as expected when the
// overflow bubble is open.
TEST_F(ShelfViewOverflowFocusTest, ForwardCyclingWithBubbleOpen) {
  OpenOverflow();

  // Focus the last item on the main shelf.
  shelf_view_->shelf_widget()->GetFocusManager()->SetFocusedView(
      test_api_->GetViewAt(last_item_on_main_shelf_index_));

  // Tests that after pressing tab once more, the overflow bubble widget now is
  // active, and the first item on the overflow bubble shelf has focus.
  DoTab();
  EXPECT_TRUE(
      test_api_->overflow_bubble()->bubble_view()->GetWidget()->IsActive());
  const int first_index_overflow_shelf = last_item_on_main_shelf_index_ + 1;
  EXPECT_TRUE(overflow_shelf_test_api_->GetViewAt(first_index_overflow_shelf)
                  ->HasFocus());

  // Focus the last item on the overflow shelf.
  test_api_->overflow_bubble()
      ->bubble_view()
      ->GetWidget()
      ->GetFocusManager()
      ->SetFocusedView(
          overflow_shelf_test_api_->GetViewAt(first_index_overflow_shelf + 3));

  // Tests that after pressing tab once more, the main shelf widget now is
  // active, and the first item on the main shelf has focus.
  DoTab();
  EXPECT_TRUE(shelf_view_->shelf_widget()->IsActive());
  EXPECT_TRUE(test_api_->GetViewAt(1)->HasFocus());
}

// Tests that backwards cycling through elements with shift tab works as
// expected when the overflow bubble is open.
TEST_F(ShelfViewOverflowFocusTest, BackwardCyclingWithBubbleOpen) {
  OpenOverflow();

  // Tests that after pressing shift tab once, the overflow shelf bubble is
  // active and the last item on the overflow shelf has focus.
  DoShiftTab();
  EXPECT_TRUE(
      test_api_->overflow_bubble()->bubble_view()->GetWidget()->IsActive());
  const int first_index_overflow_shelf = last_item_on_main_shelf_index_ + 1;
  EXPECT_TRUE(
      overflow_shelf_test_api_->GetViewAt(first_index_overflow_shelf + 3)
          ->HasFocus());

  // Focus the first item on the overflow shelf.
  test_api_->overflow_bubble()
      ->bubble_view()
      ->GetWidget()
      ->GetFocusManager()
      ->SetFocusedView(
          overflow_shelf_test_api_->GetViewAt(first_index_overflow_shelf));

  // Tests that after pressing shift tab once, the main shelf is active and
  // the last item on the main shelf has focus.
  DoShiftTab();
  EXPECT_TRUE(shelf_view_->shelf_widget()->IsActive());
  EXPECT_TRUE(test_api_->GetViewAt(last_item_on_main_shelf_index_)->HasFocus());
}

}  // namespace ash
