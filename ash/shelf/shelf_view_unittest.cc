// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_view.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/focus_cycler.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_context_menu_model.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_metrics.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/utility/haptics_tracking_test_input_controller.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/common/events.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/test/ink_drop_host_test_api.h"
#include "ui/views/animation/test/ink_drop_impl_test_api.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace ash {
namespace {

// Create a test 1x1 icon image with a given |color|.
gfx::ImageSkia CreateImageSkiaIcon(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

void ExpectFocused(views::View* view) {
  EXPECT_TRUE(view->GetWidget()->IsActive());
  EXPECT_TRUE(view->Contains(view->GetFocusManager()->GetFocusedView()));
}

void ExpectNotFocused(views::View* view) {
  EXPECT_FALSE(view->GetWidget()->IsActive());
  EXPECT_FALSE(view->Contains(view->GetFocusManager()->GetFocusedView()));
}

class LayerAnimationWaiter : public ui::LayerAnimationObserver {
 public:
  explicit LayerAnimationWaiter(ui::LayerAnimator* animator)
      : animator_(animator) {
    animator_->AddObserver(this);
  }

  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    OnAnimationCompleted();
  }

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {
    OnAnimationCompleted();
  }

  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  void Wait() { run_loop_.Run(); }

 private:
  void OnAnimationCompleted() {
    if (animator_->is_animating() == false) {
      animator_->RemoveObserver(this);
      run_loop_.Quit();
    }
  }

  // Dangling when executing ShelfViewPromiseAppTest.PromiseIconLayers because
  // layer is destroyed by the ShelfView.
  raw_ptr<ui::LayerAnimator, DanglingUntriaged> animator_;
  base::RunLoop run_loop_;
};

class TestShelfObserver : public ShelfObserver {
 public:
  explicit TestShelfObserver(Shelf* shelf) : shelf_(shelf) {
    shelf_->AddObserver(this);
  }

  TestShelfObserver(const TestShelfObserver&) = delete;
  TestShelfObserver& operator=(const TestShelfObserver&) = delete;

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
    icon_positions_animation_duration_ = base::TimeDelta();
  }
  base::TimeDelta icon_positions_animation_duration() const {
    return icon_positions_animation_duration_;
  }

 private:
  const raw_ptr<Shelf> shelf_;
  bool icon_positions_changed_ = false;
  base::TimeDelta icon_positions_animation_duration_;
};

// A ShelfItemDelegate that tracks the last context menu request, and exposes a
// method tests can use to finish the tracked request.
class AsyncContextMenuShelfItemDelegate : public ShelfItemDelegate {
 public:
  AsyncContextMenuShelfItemDelegate() : ShelfItemDelegate(ShelfID()) {}
  ~AsyncContextMenuShelfItemDelegate() override = default;

  bool RunPendingContextMenuCallback(
      std::unique_ptr<ui::SimpleMenuModel> model) {
    if (pending_context_menu_callback_.is_null()) {
      return false;
    }
    std::move(pending_context_menu_callback_).Run(std::move(model));
    return true;
  }
  bool HasPendingContextMenuCallback() const {
    return !pending_context_menu_callback_.is_null();
  }

 private:
  // ShelfItemDelegate:
  void GetContextMenu(int64_t display_id,
                      GetContextMenuCallback callback) override {
    ASSERT_TRUE(pending_context_menu_callback_.is_null());
    pending_context_menu_callback_ = std::move(callback);
  }
  void ExecuteCommand(bool, int64_t, int32_t, int64_t) override {}
  void Close() override {}

  GetContextMenuCallback pending_context_menu_callback_;
};

// ProgressIndicatorWaiter -----------------------------------------------------

// A class which supports waiting for a progress indicator to reach a desired
// state of progress.
class ProgressIndicatorWaiter {
 public:
  ProgressIndicatorWaiter() = default;
  ProgressIndicatorWaiter(const ProgressIndicatorWaiter&) = delete;
  ProgressIndicatorWaiter& operator=(const ProgressIndicatorWaiter&) = delete;
  ~ProgressIndicatorWaiter() = default;

  // Waits for `progress_indicator` to reach the specified `progress`. If the
  // `progress_indicator` is already at `progress`, this method no-ops.
  void WaitForProgress(ProgressIndicator* progress_indicator,
                       const std::optional<float>& progress) {
    if (progress_indicator->progress() == progress) {
      return;
    }
    base::RunLoop run_loop;
    auto subscription = progress_indicator->AddProgressChangedCallback(
        base::BindLambdaForTesting([&]() {
          if (progress_indicator->progress() == progress) {
            run_loop.Quit();
          }
        }));
    run_loop.Run();
  }
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ShelfObserver::OnShelfIconPositionsChanged tests.

class ShelfObserverIconTest : public AshTestBase {
 public:
  ShelfObserverIconTest() = default;

  ShelfObserverIconTest(const ShelfObserverIconTest&) = delete;
  ShelfObserverIconTest& operator=(const ShelfObserverIconTest&) = delete;

  ~ShelfObserverIconTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    observer_ = std::make_unique<TestShelfObserver>(GetPrimaryShelf());
  }

  void TearDown() override {
    observer_.reset();
    AshTestBase::TearDown();
  }

  TestShelfObserver* observer() { return observer_.get(); }

 private:
  std::unique_ptr<TestShelfObserver> observer_;
};

// A ShelfItemDelegate that tracks selections and reports a custom action.
class ShelfItemSelectionTracker : public ShelfItemDelegate {
 public:
  ShelfItemSelectionTracker() : ShelfItemDelegate(ShelfID()) {}

  ShelfItemSelectionTracker(const ShelfItemSelectionTracker&) = delete;
  ShelfItemSelectionTracker& operator=(const ShelfItemSelectionTracker&) =
      delete;

  ~ShelfItemSelectionTracker() override = default;

  size_t item_selected_count() const { return item_selected_count_; }
  void set_item_selected_action(ShelfAction item_selected_action) {
    item_selected_action_ = item_selected_action;
  }

  // ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override {
    item_selected_count_++;
    std::move(callback).Run(item_selected_action_, {});
  }
  void ExecuteCommand(bool, int64_t, int32_t, int64_t) override {}
  void Close() override {}

 private:
  size_t item_selected_count_ = 0;
  ShelfAction item_selected_action_ = SHELF_ACTION_NONE;
};

// A ShelfItemDelegate that opens system modal window on activation.
class SystemModalWindowShelfItemDelegate : public ShelfItemDelegate {
 public:
  using WindowCreator = base::RepeatingCallback<std::unique_ptr<
      aura::Window>(const gfx::Rect&, aura::client::WindowType, int)>;
  explicit SystemModalWindowShelfItemDelegate(
      const WindowCreator& window_creator)
      : ShelfItemDelegate(ShelfID()), window_creator_(window_creator) {}
  SystemModalWindowShelfItemDelegate(
      const SystemModalWindowShelfItemDelegate&) = delete;
  SystemModalWindowShelfItemDelegate& operator=(
      const SystemModalWindowShelfItemDelegate&) = delete;
  ~SystemModalWindowShelfItemDelegate() override = default;

  // ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override {
    test_window_ = window_creator_.Run(gfx::Rect(gfx::Size(50, 50)),
                                       aura::client::WINDOW_TYPE_NORMAL,
                                       kShellWindowId_Invalid);
    test_window_->SetProperty(aura::client::kModalKey,
                              ui::mojom::ModalType::kSystem);
    test_window_->Show();
    aura::client::ParentWindowWithContext(
        test_window_.get(), Shell::GetPrimaryRootWindow(), gfx::Rect(),
        display::kInvalidDisplayId);

    std::move(callback).Run(SHELF_ACTION_NONE, {});
  }
  void ExecuteCommand(bool, int64_t, int32_t, int64_t) override {}
  void Close() override {}

  bool HasTestWindow() const { return !!test_window_; }

  void ResetTestWindow() { test_window_.reset(); }

 private:
  const WindowCreator window_creator_;

  std::unique_ptr<aura::Window> test_window_;
};

// A ShelfItemDelegate to generate empty shelf context menu.
class EmptyContextMenuBuilder : public ShelfItemDelegate {
 public:
  EmptyContextMenuBuilder() : ShelfItemDelegate(ShelfID()) {}
  EmptyContextMenuBuilder(const EmptyContextMenuBuilder&) = delete;
  EmptyContextMenuBuilder& operator=(const EmptyContextMenuBuilder&) = delete;
  ~EmptyContextMenuBuilder() override = default;

  // ShelfItemDelegate:
  void GetContextMenu(int64_t display_id,
                      GetContextMenuCallback callback) override {
    std::move(callback).Run(std::make_unique<ui::SimpleMenuModel>(nullptr));
  }
  void ExecuteCommand(bool, int64_t, int32_t, int64_t) override {}
  void Close() override {}
};

TEST_F(ShelfObserverIconTest, AddRemove) {
  SetShelfAnimationDuration(base::Milliseconds(1));

  ShelfItem item;
  item.id = ShelfID("foo");
  item.type = TYPE_APP;
  EXPECT_FALSE(observer()->icon_positions_changed());
  const int shelf_item_index = ShelfModel::Get()->Add(
      item, std::make_unique<TestShelfItemDelegate>(item.id));
  WaitForShelfAnimation();
  EXPECT_TRUE(observer()->icon_positions_changed());
  observer()->Reset();

  EXPECT_FALSE(observer()->icon_positions_changed());
  ShelfModel::Get()->RemoveItemAt(shelf_item_index);
  WaitForShelfAnimation();
  EXPECT_TRUE(observer()->icon_positions_changed());
  observer()->Reset();
}

// Make sure creating/deleting an window on one displays notifies a
// shelf on external display as well as one on primary.
TEST_F(ShelfObserverIconTest, AddRemoveWithMultipleDisplays) {
  UpdateDisplay("500x400,500x400");
  SetShelfAnimationDuration(base::Milliseconds(1));

  observer()->Reset();

  TestShelfObserver second_observer(
      Shelf::ForWindow(Shell::GetAllRootWindows()[1]));

  ShelfItem item;
  item.id = ShelfID("foo");
  item.type = TYPE_APP;
  EXPECT_FALSE(observer()->icon_positions_changed());
  EXPECT_FALSE(second_observer.icon_positions_changed());

  // Add item and wait for all animations to finish.
  const int shelf_item_index = ShelfModel::Get()->Add(
      item, std::make_unique<TestShelfItemDelegate>(item.id));
  WaitForShelfAnimation();

  EXPECT_TRUE(observer()->icon_positions_changed());
  EXPECT_TRUE(second_observer.icon_positions_changed());

  // Reset observer so they can track the next set of animations.
  observer()->Reset();
  ASSERT_FALSE(observer()->icon_positions_changed());
  second_observer.Reset();
  ASSERT_FALSE(second_observer.icon_positions_changed());

  // Remove the item, and wait for all the animations to complete.
  ShelfModel::Get()->RemoveItemAt(shelf_item_index);
  WaitForShelfAnimation();

  EXPECT_TRUE(observer()->icon_positions_changed());
  EXPECT_TRUE(second_observer.icon_positions_changed());

  observer()->Reset();
  second_observer.Reset();
}

////////////////////////////////////////////////////////////////////////////////
// ShelfView tests.

class ShelfViewTest : public AshTestBase {
 public:
  static const char*
      kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName;

  template <typename... TaskEnvironmentTraits>
  explicit ShelfViewTest(TaskEnvironmentTraits&&... traits)
      : AshTestBase(std::forward<TaskEnvironmentTraits>(traits)...) {}
  ShelfViewTest(const ShelfViewTest&) = delete;
  ShelfViewTest& operator=(const ShelfViewTest&) = delete;
  ~ShelfViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    haptics_tracker_ = std::make_unique<HapticsTrackingTestInputController>();
    model_ = ShelfModel::Get();
    shelf_view_ = GetPrimaryShelf()->GetShelfViewForTesting();
    navigation_view_ = GetPrimaryShelf()
                           ->shelf_widget()
                           ->navigation_widget()
                           ->GetContentsView();
    gfx::NativeWindow window = shelf_view_->shelf_widget()->GetNativeWindow();
    status_area_ = RootWindowController::ForWindow(window)
                       ->GetStatusAreaWidget()
                       ->GetContentsView();

    // If the desk button is enabled there will be less space for buttons.
    if (!features::IsDeskButtonEnabled()) {
      // The bounds should be big enough for 4 buttons.
      ASSERT_GE(GetPrimaryShelf()
                    ->shelf_widget()
                    ->hotseat_widget()
                    ->GetWindowBoundsInScreen()
                    .width(),
                500);
    }

    test_api_ = std::make_unique<ShelfViewTestAPI>(shelf_view_);
    test_api_->SetAnimationDuration(base::Milliseconds(1));

    // Add a browser shortcut shelf item, as chrome does, for testing.
    AddItem(TYPE_BROWSER_SHORTCUT, true);
  }

  void TearDown() override {
    test_api_.reset();
    haptics_tracker_.reset();
    AshTestBase::TearDown();
  }

  std::string GetNextAppId() const { return base::NumberToString(id_); }

 protected:
  // Add shelf items of various types, and optionally wait for animations.
  ShelfID AddItem(ShelfItemType type, bool wait_for_animations) {
    ShelfItem item = ShelfTestUtil::AddAppShortcutWithIcon(
        base::NumberToString(id_++), type, CreateImageSkiaIcon(SK_ColorRED));
    // Set a delegate; some tests require one to select the item.
    model_->ReplaceShelfItemDelegate(
        item.id, std::make_unique<ShelfItemSelectionTracker>());
    if (wait_for_animations) {
      test_api_->RunMessageLoopUntilAnimationsDone();
    }
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

  ShelfAppButton* GetButtonByID(const ShelfID& id) {
    return test_api_->GetButton(model_->ItemIndexByID(id));
  }

  ShelfItem GetItemByID(const ShelfID& id) { return *model_->ItemByID(id); }

  bool IsAppPinned(const ShelfID& id) const {
    return model_->IsAppPinned(id.app_id);
  }

  void CheckModelIDs(
      const std::vector<std::pair<ShelfID, views::View*>>& id_map) {
    size_t map_index = 0;
    for (size_t model_index = 0; model_index < model_->items().size();
         ++model_index) {
      ShelfItem item = model_->items()[model_index];
      ShelfID id = item.id;
      EXPECT_EQ(id_map[map_index].first, id);
      ++map_index;
    }
    ASSERT_EQ(map_index, id_map.size());
  }

  void ExpectHelpBubbleAnchorBoundsChangedEvent(
      base::FunctionRef<void()> function_ref) {
    base::RunLoop run_loop;

    auto subscription =
        ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
            user_education::kHelpBubbleAnchorBoundsChangedEvent,
            views::ElementTrackerViews::GetContextForView(shelf_view_),
            base::BindLambdaForTesting(
                [&](ui::TrackedElement* tracked_element) {
                  if (tracked_element->IsA<views::TrackedElementViews>() &&
                      tracked_element->AsA<views::TrackedElementViews>()
                              ->view() == shelf_view_) {
                    run_loop.Quit();
                  }
                }));

    function_ref();
    run_loop.Run();
  }

  void VerifyAnchorBoundsInScreenAreValid() {
    gfx::Rect anchor_bounds_in_screen;
    for (int i : shelf_view_->visible_views_indices()) {
      if (test_api_->GetButton(i)) {
        anchor_bounds_in_screen.Union(
            test_api_->GetViewAt(i)->GetBoundsInScreen());
      }
    }
    if (shelf_view_->parent()) {
      anchor_bounds_in_screen.Intersect(
          shelf_view_->parent()->GetBoundsInScreen());
    }
    EXPECT_THAT(shelf_view_->GetAnchorBoundsInScreen(),
                ::testing::Conditional(anchor_bounds_in_screen.IsEmpty(),
                                       shelf_view_->GetBoundsInScreen(),
                                       anchor_bounds_in_screen));
  }

  void VerifyShelfItemBoundsAreValid() {
    for (int i : shelf_view_->visible_views_indices()) {
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
    ui::MouseEvent pressed_event(ui::EventType::kMousePressed, gfx::Point(),
                                 view->GetBoundsInScreen().origin(),
                                 ui::EventTimeForNow(), 0, 0);
    shelf_view_->PointerPressedOnButton(view, pointer, pressed_event);
    return view;
  }

  // Similar to SimulateViewPressed, but the index must not be for the app list,
  // since the home button is not a ShelfAppButton.
  ShelfAppButton* SimulateButtonPressed(ShelfView::Pointer pointer,
                                        int button_index) {
    ShelfAppButton* button = test_api_->GetButton(button_index);
    EXPECT_EQ(button, SimulateViewPressed(pointer, button_index));
    return button;
  }

  // Simulates a single mouse click.
  void SimulateClick(int button_index) {
    ShelfAppButton* button =
        SimulateButtonPressed(ShelfView::MOUSE, button_index);
    ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(),
                                 button->GetBoundsInScreen().origin(),
                                 ui::EventTimeForNow(), 0, 0);
    button->NotifyClick(release_event);
    button->OnMouseReleased(release_event);
  }

  // Simulates the second click of a double click.
  void SimulateDoubleClick(int button_index) {
    ShelfAppButton* button =
        SimulateButtonPressed(ShelfView::MOUSE, button_index);
    ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(),
                                 button->GetBoundsInScreen().origin(),
                                 ui::EventTimeForNow(), ui::EF_IS_DOUBLE_CLICK,
                                 0);
    button->NotifyClick(release_event);
    button->OnMouseReleased(release_event);
  }

  void DoDrag(int dist_x,
              int dist_y,
              views::View* button,
              ShelfView::Pointer pointer,
              views::View* to) {
    ui::MouseEvent drag_event(
        ui::EventType::kMouseDragged, gfx::Point(dist_x, dist_y),
        to->GetBoundsInScreen().origin(), ui::EventTimeForNow(), 0, 0);
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
    gfx::Rect to_rect = to->GetMirroredBounds();
    gfx::Rect from_rect = from->GetMirroredBounds();
    int dist_x = to_rect.x() - from_rect.x();
    int dist_y = to_rect.y() - from_rect.y();
    if (progressively) {
      int sgn = dist_x > 0 ? 1 : -1;
      dist_x = abs(dist_x);
      for (; dist_x; dist_x -= std::min(10, dist_x)) {
        DoDrag(sgn * std::min(10, abs(dist_x)), 0, button, pointer, to);
      }
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
           cur_index++) {
        ContinueDrag(button, pointer, cur_index - 1, cur_index, true);
      }
    } else if (button_index > destination_index) {
      for (int cur_index = button_index - 1; cur_index >= destination_index;
           cur_index--) {
        ContinueDrag(button, pointer, cur_index + 1, cur_index, true);
      }
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
      ShelfAppButton* button = test_api_->GetButton(i);
      id_map->push_back(std::make_pair(model_->items()[i].id, button));
    }
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(*id_map));

    // Add 5 app shelf buttons for testing.
    for (int i = 0; i < 5; ++i) {
      ShelfID id = AddAppShortcut();
      // The browser shortcut is located at index 0. So we should start to add
      // app shortcuts at index 1.
      id_map->insert(id_map->begin() + i + 1,
                     std::make_pair(id, GetButtonByID(id)));
    }
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(*id_map));
  }

  // Returns the item's ShelfID at |index|.
  ShelfID GetItemId(int index) const {
    DCHECK_GE(index, 0);
    return model_->items()[index].id;
  }

  // Returns the center point of a button. Helper function for event generators.
  gfx::Point GetButtonCenter(const ShelfID& button_id) {
    return GetButtonCenter(GetButtonByID(button_id));
  }

  gfx::Point GetButtonCenter(const ShelfAppButton* button) {
    return button->GetBoundsInScreen().CenterPoint();
  }

  int GetHapticTickEventsCount() const {
    return haptics_tracker_->GetSentHapticCount(
        ui::HapticTouchpadEffect::kTick,
        ui::HapticTouchpadEffectStrength::kMedium);
  }

  raw_ptr<ShelfModel, DanglingUntriaged> model_ = nullptr;
  raw_ptr<ShelfView, DanglingUntriaged> shelf_view_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> navigation_view_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> status_area_ = nullptr;

  int id_ = 0;

  // Used to track haptics events sent during drag.
  std::unique_ptr<HapticsTrackingTestInputController> haptics_tracker_;
  std::unique_ptr<ShelfViewTestAPI> test_api_;
};

TEST_F(ShelfViewTest, AccessibleProperties) {
  ui::AXNodeData data;

  shelf_view_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kToolbar);
  EXPECT_EQ(data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF8(IDS_ASH_SHELF_ACCESSIBLE_NAME));
}

class LtrRtlShelfViewTest : public ShelfViewTest,
                            public testing::WithParamInterface<bool> {
 public:
  LtrRtlShelfViewTest() : scoped_locale_(GetParam() ? "he" : "") {}
  LtrRtlShelfViewTest(const LtrRtlShelfViewTest&) = delete;
  LtrRtlShelfViewTest& operator=(const LtrRtlShelfViewTest&) = delete;
  ~LtrRtlShelfViewTest() override = default;

  bool IsRtlEnabled() const { return GetParam(); }

 private:
  // Restores locale to the default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;
};

INSTANTIATE_TEST_SUITE_P(All, LtrRtlShelfViewTest, testing::Bool());

const char*
    ShelfViewTest::kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName =
        ShelfButtonPressedMetricTracker::
            kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName;

TEST_P(LtrRtlShelfViewTest, GetAnchorBoundsInScreen) {
  // Help bubble anchor bounds changed events are only propagated when user
  // education features are enabled.
  base::test::ScopedFeatureList scoped_feature_list(features::kWelcomeTour);

  {
    SCOPED_TRACE("Initial anchor bounds.");
    VerifyAnchorBoundsInScreenAreValid();
  }

  ShelfID app_shortcut_id;

  {
    SCOPED_TRACE("Update anchor bounds due to addition.");
    ExpectHelpBubbleAnchorBoundsChangedEvent(
        [&]() { app_shortcut_id = AddAppShortcut(); });
    VerifyAnchorBoundsInScreenAreValid();
  }

  {
    SCOPED_TRACE("Update anchor bounds due to removal.");
    ExpectHelpBubbleAnchorBoundsChangedEvent(
        [&]() { RemoveByID(app_shortcut_id); });
    VerifyAnchorBoundsInScreenAreValid();
  }

  {
    SCOPED_TRACE("Shelf overflow anchor bounds.");
    ExpectHelpBubbleAnchorBoundsChangedEvent([&]() {
      while (shelf_view_->parent()->bounds().width() >=
             shelf_view_->bounds().width()) {
        AddAppShortcut();
      }
    });
    VerifyAnchorBoundsInScreenAreValid();
  }
}

TEST_P(LtrRtlShelfViewTest, VisibleShelfItemsBounds) {
  // Add 3 pinned apps, and a normal app.
  AddAppShortcut();
  AddAppShortcut();
  AddAppShortcut();
  const auto app_id = AddApp();

  EXPECT_EQ(static_cast<size_t>(model_->item_count()),
            shelf_view_->number_of_visible_apps());
  const gfx::Rect visible_items_bounds =
      test_api_->visible_shelf_item_bounds_union();

  // Pin the app with `app_id` and expect that the visible items bounds union
  // remains the same.
  SetShelfItemTypeToAppShortcut(app_id);
  EXPECT_EQ(static_cast<size_t>(model_->item_count()),
            shelf_view_->number_of_visible_apps());
  EXPECT_EQ(visible_items_bounds, test_api_->visible_shelf_item_bounds_union());
}

// Checks that shelf view contents are considered in the correct drag group.
TEST_P(LtrRtlShelfViewTest, EnforceDragType) {
  EXPECT_TRUE(test_api_->SameDragType(TYPE_APP, TYPE_APP));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP, TYPE_PINNED_APP));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP, TYPE_BROWSER_SHORTCUT));
  EXPECT_FALSE(
      test_api_->SameDragType(TYPE_APP, TYPE_UNPINNED_BROWSER_SHORTCUT));

  EXPECT_TRUE(test_api_->SameDragType(TYPE_PINNED_APP, TYPE_PINNED_APP));
  EXPECT_TRUE(test_api_->SameDragType(TYPE_PINNED_APP, TYPE_BROWSER_SHORTCUT));
  EXPECT_FALSE(
      test_api_->SameDragType(TYPE_PINNED_APP, TYPE_UNPINNED_BROWSER_SHORTCUT));

  EXPECT_TRUE(
      test_api_->SameDragType(TYPE_BROWSER_SHORTCUT, TYPE_BROWSER_SHORTCUT));
  EXPECT_TRUE(test_api_->SameDragType(TYPE_UNPINNED_BROWSER_SHORTCUT,
                                      TYPE_UNPINNED_BROWSER_SHORTCUT));

  // No test for TYPE_BROWSER_SHORTCUT and TYPE_UNPINNED_BROWSER_SHORTCUT,
  // because they should be mutually exclusive.
}

// Check that model changes are handled correctly while a shelf icon is being
// dragged.
TEST_P(LtrRtlShelfViewTest, ModelChangesWhileDragging) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  // Dragging browser shortcut at index 1.
  EXPECT_TRUE(model_->items()[0].type == TYPE_BROWSER_SHORTCUT);
  views::View* dragged_button = SimulateDrag(ShelfView::MOUSE, 0, 2, false);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  std::rotate(id_map.begin(), id_map.begin() + 1, id_map.begin() + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  EXPECT_TRUE(model_->items()[2].type == TYPE_BROWSER_SHORTCUT);
  test_api_->RunMessageLoopUntilAnimationsDone();

  // Dragging changes model order.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 0, 2, false);
  EXPECT_EQ(2, GetHapticTickEventsCount());
  std::rotate(id_map.begin(), id_map.begin() + 1, id_map.begin() + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Cancelling the drag operation restores previous order.
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, true);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(2, GetHapticTickEventsCount());
  std::rotate(id_map.begin(), id_map.begin() + 2, id_map.begin() + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Deleting an item keeps the remaining intact.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 0, 2, false);
  EXPECT_EQ(3, GetHapticTickEventsCount());

  // The dragged view has been moved to index 2 during drag.
  std::rotate(id_map.begin(), id_map.begin() + 1, id_map.begin() + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  model_->RemoveItemAt(2);
  id_map.erase(id_map.begin() + 2);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);

  // Waits until app removal animation finishes.
  test_api_->RunMessageLoopUntilAnimationsDone();

  // Adding a shelf item cancels the drag and respects the order.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 0, 2, false);
  EXPECT_EQ(4, GetHapticTickEventsCount());
  ShelfID new_id = AddAppShortcut();
  id_map.insert(id_map.begin() + 5,
                std::make_pair(new_id, GetButtonByID(new_id)));
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  EXPECT_EQ(4, GetHapticTickEventsCount());
}

// Check that 2nd drag from the other pointer would be ignored.
TEST_P(LtrRtlShelfViewTest, SimultaneousDrag) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  // Start a mouse drag.
  views::View* dragged_button_mouse =
      SimulateDrag(ShelfView::MOUSE, 2, 4, false);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  std::rotate(id_map.begin() + 2, id_map.begin() + 3, id_map.begin() + 5);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  // Attempt a touch drag before the mouse drag finishes.
  views::View* dragged_button_touch =
      SimulateDrag(ShelfView::TOUCH, 5, 3, false);
  EXPECT_EQ(1, GetHapticTickEventsCount());

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
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

// Ensure that the behavior of pinning by dragging works as expected.
TEST_F(ShelfViewTest, DragAppsToPin) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);
  size_t pinned_apps_size = id_map.size();

  const ShelfID open_app_id = AddApp();
  id_map.emplace_back(open_app_id, GetButtonByID(open_app_id));

  ASSERT_TRUE(GetButtonByID(open_app_id)->state() &
              ShelfAppButton::STATE_RUNNING);
  EXPECT_EQ(test_api_->GetSeparatorIndex(), pinned_apps_size - 1);

  // Drag the browser shortcut at index 0 and move it to the end of the shelf.
  // The browser shortcut are not allowed to be moved across the separator so
  // the dragged browser shortcut will stay pinned beside the separator after
  // release.
  views::View* dragged_button =
      SimulateDrag(ShelfView::MOUSE, 0, id_map.size() - 1, false);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  std::rotate(id_map.begin(), id_map.begin() + 1,
              id_map.begin() + pinned_apps_size);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, GetHapticTickEventsCount());
  EXPECT_TRUE(IsAppPinned(id_map[pinned_apps_size - 1].first));
  EXPECT_EQ(test_api_->GetSeparatorIndex(), pinned_apps_size - 1);

  // Drag the app at index 1 and move it to the end of the shelf. The pinned
  // apps are not allowed to be moved across the separator so the dragged app
  // will stay pinned beside the separator after release.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 1, id_map.size() - 1, false);
  EXPECT_EQ(2, GetHapticTickEventsCount());
  std::rotate(id_map.begin() + 1, id_map.begin() + 2,
              id_map.begin() + pinned_apps_size);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(2, GetHapticTickEventsCount());
  EXPECT_TRUE(IsAppPinned(id_map[pinned_apps_size - 1].first));
  EXPECT_EQ(test_api_->GetSeparatorIndex(), pinned_apps_size - 1);

  // Drag an app in unpinned app side and move it to the beginning of the shelf.
  // With separator available and the app is dragged to the pinned app side, the
  // dragged app should be pinned and moved to the released position.
  dragged_button = SimulateDrag(ShelfView::MOUSE, id_map.size() - 1, 0, false);
  EXPECT_EQ(3, GetHapticTickEventsCount());
  std::rotate(id_map.rbegin(), id_map.rbegin() + 1, id_map.rend());
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(3, GetHapticTickEventsCount());
  EXPECT_TRUE(IsAppPinned(id_map[0].first));
  ++pinned_apps_size;

  // After pinning the last unpinned app by dragging, the separator is removed
  // as there is no unpinned app on the shelf.
  EXPECT_EQ(test_api_->GetSeparatorIndex(), std::nullopt);
}

// Ensure that the unpinnable apps can not be pinned by dragging.
TEST_F(ShelfViewTest, NotPinnableItemCantBePinnedByDragging) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);
  size_t pinned_apps_size = id_map.size();

  // Add an unpinnable app.
  const ShelfItem unpinnable_app =
      ShelfTestUtil::AddAppNotPinnable(base::NumberToString(id_++));
  const ShelfID id = unpinnable_app.id;
  id_map.emplace_back(id, GetButtonByID(id));

  ASSERT_TRUE(GetButtonByID(id)->state() & ShelfAppButton::STATE_RUNNING);
  ASSERT_FALSE(IsAppPinned(id));
  EXPECT_EQ(test_api_->GetSeparatorIndex(), pinned_apps_size - 1);

  // Drag an unpinnable app and move it to the beginning of the shelf. The app
  // can not be moved across the separator so the dragged app will stay unpinned
  // beside the separator after release.
  views::View* dragged_button =
      SimulateDrag(ShelfView::MOUSE, id_map.size() - 1, 0, false);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, GetHapticTickEventsCount());
  EXPECT_EQ(test_api_->GetSeparatorIndex(), pinned_apps_size - 1);
  EXPECT_FALSE(IsAppPinned(id));
}

// Verifies that a "dialog" item is correctly detected as not pinned when
// determining the separator position, and that it cannot be dragged to pinned
// state.
TEST_F(ShelfViewTest, SeparatorCorrectlyPositionedNextToDialogItem) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);
  size_t pinned_apps_size = id_map.size();

  const ShelfID dialog_id = AddItem(TYPE_DIALOG, true);
  id_map.emplace_back(dialog_id, GetButtonByID(dialog_id));

  EXPECT_EQ(test_api_->GetSeparatorIndex(), pinned_apps_size - 1);

  // Drag an unpinnable dialog item and move it to the beginning of the shelf.
  // The item cannot be moved across the separator so the dragged item will
  // remain unpinned after release.
  views::View* dragged_button =
      SimulateDrag(ShelfView::MOUSE, id_map.size() - 1, 0, false);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, GetHapticTickEventsCount());
  EXPECT_EQ(test_api_->GetSeparatorIndex(), pinned_apps_size - 1);
  EXPECT_FALSE(IsAppPinned(dialog_id));
}

// Check that separator index updates as expected when a drag view is dragged
// over it.
TEST_F(ShelfViewTest, DragAppAroundSeparator) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);
  const size_t pinned_apps_size = id_map.size();

  const ShelfID open_app_id = AddApp();
  id_map.emplace_back(open_app_id, GetButtonByID(open_app_id));
  EXPECT_EQ(test_api_->GetSeparatorIndex(), pinned_apps_size - 1);
  const int button_width =
      GetButtonByID(open_app_id)->GetBoundsInScreen().width();

  ui::test::EventGenerator* generator = GetEventGenerator();

  // The test makes some assumptions that the shelf is bottom aligned.
  ASSERT_EQ(shelf_view_->shelf()->alignment(), ShelfAlignment::kBottom);

  // Drag an unpinned open app that is beside the separator around and check
  // that the separator is correctly placed.
  ASSERT_EQ(static_cast<size_t>(model_->ItemIndexByID(open_app_id)),
            test_api_->GetSeparatorIndex().value() + 1);
  gfx::Point unpinned_app_location =
      GetButtonCenter(GetButtonByID(open_app_id));
  generator->set_current_screen_location(unpinned_app_location);
  generator->PressLeftButton();
  // Drag the mouse slightly to the left. The dragged app will stay at the same
  // index but the separator will move to the right.
  generator->MoveMouseBy(-button_width / 4, 0);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  // In this case, the separator is moved to the end of the shelf so it is set
  // invisible and the |separator_index_| will be updated to nullopt.
  EXPECT_FALSE(test_api_->IsSeparatorVisible());
  EXPECT_FALSE(test_api_->GetSeparatorIndex().has_value());
  // Drag the mouse slightly to the right where the dragged app will stay at the
  // same index.
  generator->MoveMouseBy(button_width / 2, 0);
  // In this case, because the dragged app is not released or pinned yet,
  // dragging it back to its original place will show the separator again.
  EXPECT_EQ(test_api_->GetSeparatorIndex(), pinned_apps_size - 1);
  generator->ReleaseLeftButton();
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

// Ensure that clicking on one item and then dragging another works as expected.
TEST_P(LtrRtlShelfViewTest, ClickOneDragAnother) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  // A click on the item at index 1 is simulated.
  SimulateClick(1);

  // Dragging the browser item at index 0 should change the model order.
  EXPECT_TRUE(model_->items()[0].type == TYPE_BROWSER_SHORTCUT);
  views::View* dragged_button = SimulateDrag(ShelfView::MOUSE, 0, 2, false);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  std::rotate(id_map.begin(), id_map.begin() + 1, id_map.begin() + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  EXPECT_TRUE(model_->items()[2].type == TYPE_BROWSER_SHORTCUT);
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

// Tests that double-clicking an item does not activate it twice.
TEST_P(LtrRtlShelfViewTest, ClickingTwiceActivatesOnce) {
  // Watch for selection of the browser shortcut.
  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->ReplaceShelfItemDelegate(model_->items()[0].id,
                                   base::WrapUnique(selection_tracker));

  // A single click selects the item, but a double-click does not.
  EXPECT_EQ(0u, selection_tracker->item_selected_count());
  SimulateClick(0);
  EXPECT_EQ(1u, selection_tracker->item_selected_count());
  SimulateDoubleClick(0);
  EXPECT_EQ(1u, selection_tracker->item_selected_count());
  EXPECT_EQ(0, GetHapticTickEventsCount());
}

// Check that very small mouse drags do not prevent shelf item selection.
TEST_P(LtrRtlShelfViewTest, ClickAndMoveSlightly) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  ShelfID shelf_id = (id_map.begin() + 2)->first;
  views::View* button = (id_map.begin() + 2)->second;

  // Install a ShelfItemDelegate that tracks when the shelf item is selected.
  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->ReplaceShelfItemDelegate(
      shelf_id, base::WrapUnique<ShelfItemSelectionTracker>(selection_tracker));

  gfx::Vector2d press_offset(5, 30);
  gfx::Point press_location = gfx::Point() + press_offset;
  gfx::Point press_location_in_screen =
      button->GetBoundsInScreen().origin() + press_offset;

  ui::MouseEvent click_event(ui::EventType::kMousePressed, press_location,
                             press_location_in_screen, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(click_event);
  EXPECT_EQ(0u, selection_tracker->item_selected_count());

  ui::MouseEvent drag_event1(
      ui::EventType::kMouseDragged, press_location + gfx::Vector2d(0, 1),
      press_location_in_screen + gfx::Vector2d(0, 1), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event1);
  ui::MouseEvent drag_event2(
      ui::EventType::kMouseDragged, press_location + gfx::Vector2d(-1, 0),
      press_location_in_screen + gfx::Vector2d(-1, 0), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event2);
  EXPECT_EQ(0u, selection_tracker->item_selected_count());

  ui::MouseEvent release_event(
      ui::EventType::kMouseReleased, press_location + gfx::Vector2d(-1, 0),
      press_location_in_screen + gfx::Vector2d(-1, 0), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
  EXPECT_EQ(1u, selection_tracker->item_selected_count());
  EXPECT_EQ(0, GetHapticTickEventsCount());
}

// Confirm that item status changes are reflected in the buttons.
TEST_P(LtrRtlShelfViewTest, ShelfItemStatus) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), shelf_view_->number_of_visible_apps());

  // Add platform app button.
  ShelfID last_added = AddApp();
  ShelfItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  ShelfAppButton* button = GetButtonByID(last_added);
  ASSERT_EQ(ShelfAppButton::STATE_RUNNING, button->state());
  item.status = STATUS_ATTENTION;
  model_->Set(index, item);
  ASSERT_EQ(ShelfAppButton::STATE_ATTENTION, button->state());
}

// Test what drag movements will rip an item off the shelf.
TEST_P(LtrRtlShelfViewTest, ShelfRipOff) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // The test makes some assumptions that the shelf is bottom aligned.
  ASSERT_EQ(shelf_view_->shelf()->alignment(), ShelfAlignment::kBottom);

  // The rip off threshold. Taken from |kRipOffDistance| in shelf_view.cc.
  const int kRipOffDistance = 48;

  // Add two apps on the main shelf.
  ShelfID first_app_id = AddAppShortcut();
  ShelfID second_app_id = AddAppShortcut();

  // Verify that dragging an app off the shelf will trigger the app getting
  // ripped off, unless the distance is less than |kRipOffDistance|.
  gfx::Point first_app_location = GetButtonCenter(GetButtonByID(first_app_id));
  generator->set_current_screen_location(first_app_location);
  generator->PressLeftButton();
  // Drag the mouse to just off the shelf.
  generator->MoveMouseBy(0, -ShelfConfig::Get()->shelf_size() / 2 - 1);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  EXPECT_FALSE(test_api_->IsRippedOffFromShelf());
  // Drag the mouse past the rip off threshold.
  generator->MoveMouseBy(0, -kRipOffDistance);
  EXPECT_TRUE(test_api_->IsRippedOffFromShelf());
  // Drag the mouse back to the original position, so that the app does not get
  // deleted.
  generator->MoveMouseTo(first_app_location);
  generator->ReleaseLeftButton();
  EXPECT_EQ(1, GetHapticTickEventsCount());
  EXPECT_FALSE(test_api_->IsRippedOffFromShelf());
}

// Test that rip off drag can gracefully be canceled.
TEST_P(LtrRtlShelfViewTest, ShelfRipOffCancel) {
  // The test makes some assumptions that the shelf is bottom aligned.
  ASSERT_EQ(shelf_view_->shelf()->alignment(), ShelfAlignment::kBottom);

  // The rip off threshold. Taken from |kRipOffDistance| in shelf_view.cc.
  constexpr int kRipOffDistance = 48;

  // Add two apps on the main shelf.
  ShelfID first_app_id = AddAppShortcut();
  AddAppShortcut();

  // Cache the shelf state - test will verify that shelf items are not changed
  // if rip off drag gets canceled.
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  for (size_t i = 0; i < model_->items().size(); ++i) {
    ShelfAppButton* button = test_api_->GetButton(i);
    id_map.emplace_back(model_->items()[i].id, button);
  }

  // Verify that dragging an app off the shelf will trigger the app getting
  // ripped off, unless the distance is less than |kRipOffDistance|.
  const ShelfAppButton* dragged_button = GetButtonByID(first_app_id);
  gfx::Point first_app_location = GetButtonCenter(dragged_button);

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(first_app_location);
  generator->PressLeftButton();

  // Drag the mouse to just off the shelf.
  generator->MoveMouseBy(0, -ShelfConfig::Get()->shelf_size() / 2 - 1);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  EXPECT_FALSE(test_api_->IsRippedOffFromShelf());

  // Drag the mouse past the rip off threshold.
  generator->MoveMouseBy(0, -kRipOffDistance);
  EXPECT_TRUE(test_api_->IsRippedOffFromShelf());

  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, true);
  generator->ReleaseLeftButton();
  EXPECT_EQ(1, GetHapticTickEventsCount());

  EXPECT_FALSE(test_api_->IsRippedOffFromShelf());
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
}

// Tests that drag and drop a pinned running app will unpin it.
TEST_P(LtrRtlShelfViewTest, DragAndDropPinnedRunningApp) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // The test makes some assumptions that the shelf is bottom aligned.
  ASSERT_EQ(shelf_view_->shelf()->alignment(), ShelfAlignment::kBottom);

  // The rip off threshold. Taken from |kRipOffDistance| in shelf_view.cc.
  constexpr int kRipOffDistance = 48;

  const ShelfID id = AddApp();
  // Added only one app here, the index of the app will not change after drag
  // and drop.
  int index = model_->ItemIndexByID(id);
  ShelfItem item = GetItemByID(id);
  EXPECT_EQ(STATUS_RUNNING, item.status);
  model_->PinExistingItemWithID(id.app_id);
  EXPECT_TRUE(IsAppPinned(GetItemId(index)));

  gfx::Point app_location = GetButtonCenter(GetButtonByID(id));
  generator->set_current_screen_location(app_location);
  generator->PressLeftButton();
  generator->MoveMouseBy(0, -ShelfConfig::Get()->shelf_size() / 2 - 1);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  EXPECT_FALSE(test_api_->IsRippedOffFromShelf());
  generator->MoveMouseBy(0, -kRipOffDistance);
  EXPECT_TRUE(test_api_->IsRippedOffFromShelf());
  generator->ReleaseLeftButton();
  EXPECT_FALSE(IsAppPinned(GetItemId(index)));
  EXPECT_EQ(1, GetHapticTickEventsCount());
}

// Double click an app while animating drag icon drop.
TEST_P(LtrRtlShelfViewTest, ActivateAppButtonDuringDropAnimation) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Enable animations, as the test verifies behavior while a drop animation is
  // in progress.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // The test makes some assumptions that the shelf is bottom aligned.
  ASSERT_EQ(shelf_view_->shelf()->alignment(), ShelfAlignment::kBottom);

  const ShelfID drag_item_id = AddApp();
  const ShelfID activated_item_id = AddApp();

  // Watch for selection of the browser shortcut.
  auto owned_selection_tracker = std::make_unique<ShelfItemSelectionTracker>();
  ShelfItemSelectionTracker* selection_tracker = owned_selection_tracker.get();
  model_->ReplaceShelfItemDelegate(activated_item_id,
                                   std::move(owned_selection_tracker));

  generator->set_current_screen_location(
      GetButtonCenter(GetButtonByID(drag_item_id)));
  generator->PressLeftButton();
  generator->MoveMouseBy(0, -ShelfConfig::Get()->shelf_size() / 2 - 1);
  EXPECT_EQ(1, GetHapticTickEventsCount());
  generator->ReleaseLeftButton();
  EXPECT_EQ(1, GetHapticTickEventsCount());

  generator->set_current_screen_location(
      GetButtonCenter(GetButtonByID(activated_item_id)));
  generator->DoubleClickLeftButton();

  EXPECT_EQ(1, GetHapticTickEventsCount());
  EXPECT_EQ(1u, selection_tracker->item_selected_count());
  VerifyShelfItemBoundsAreValid();
}

// Confirm that item status changes are reflected in the buttons
// for platform apps.
TEST_P(LtrRtlShelfViewTest, ShelfItemStatusPlatformApp) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), shelf_view_->number_of_visible_apps());

  // Add platform app button.
  ShelfID last_added = AddApp();
  ShelfItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  ShelfAppButton* button = GetButtonByID(last_added);
  ASSERT_EQ(ShelfAppButton::STATE_RUNNING, button->state());
  item.status = STATUS_ATTENTION;
  model_->Set(index, item);
  ASSERT_EQ(ShelfAppButton::STATE_ATTENTION, button->state());
}

// Confirm that shelf item bounds are correctly updated on shelf changes.
TEST_P(LtrRtlShelfViewTest, ShelfItemBoundsCheck) {
  VerifyShelfItemBoundsAreValid();
  shelf_view_->shelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyShelfItemBoundsAreValid();
  shelf_view_->shelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyShelfItemBoundsAreValid();
}

TEST_P(LtrRtlShelfViewTest, ShelfTooltipTest) {
  ASSERT_EQ(shelf_view_->number_of_visible_apps(), test_api_->GetButtonCount());

  // Prepare some items to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfID platform_button_id = AddApp();

  ShelfAppButton* app_button = GetButtonByID(app_button_id);
  ShelfAppButton* platform_button = GetButtonByID(platform_button_id);

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
TEST_P(LtrRtlShelfViewTest, RemovingItemClosesTooltip) {
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();

  // Add an item to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfAppButton* app_button = GetButtonByID(app_button_id);

  // Spawn a tooltip on that item.
  tooltip_manager->ShowTooltip(app_button);
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Remove the app shortcut while the tooltip is open. The tooltip should be
  // closed.
  RemoveByID(app_button_id);
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Change the shelf layout. This should not crash.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
}

// Changing the shelf alignment closes any open tooltip.
TEST_P(LtrRtlShelfViewTest, ShelfAlignmentClosesTooltip) {
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();

  // Add an item to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfAppButton* app_button = GetButtonByID(app_button_id);

  // Spawn a tooltip on the item.
  tooltip_manager->ShowTooltip(app_button);
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Changing shelf alignment hides the tooltip.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_FALSE(tooltip_manager->IsVisible());
}

// Verifies that the time of button press is recorded correctly in clamshell.
TEST_P(LtrRtlShelfViewTest, HomeButtonMetricsInClamshell) {
  const HomeButton* home_button =
      GetPrimaryShelf()->navigation_widget()->GetHomeButton();

  // Make sure we're not showing the app list.
  EXPECT_FALSE(home_button->IsShowingAppList());

  base::UserActionTester user_action_tester;
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "AppList_HomeButtonPressedClamshell"));

  GetEventGenerator()->GestureTapAt(
      home_button->GetBoundsInScreen().CenterPoint());
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "AppList_HomeButtonPressedClamshell"));
  EXPECT_TRUE(home_button->IsShowingAppList());
}

// Verifies that the time of button press is recorded correctly in tablet.
TEST_P(LtrRtlShelfViewTest, HomeButtonMetricsInTablet) {
  // Enable accessibility feature that forces home button to be shown even with
  // kHideShelfControlsInTabletMode enabled.
  Shell::Get()
      ->accessibility_controller()
      ->SetTabletModeShelfNavigationButtonsEnabled(true);
  ash::TabletModeControllerTestApi().EnterTabletMode();

  const HomeButton* home_button =
      GetPrimaryShelf()->navigation_widget()->GetHomeButton();

  // Make sure we're not showing the app list.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  wm::ActivateWindow(window.get());
  EXPECT_FALSE(home_button->IsShowingAppList());

  base::UserActionTester user_action_tester;
  ASSERT_EQ(
      0, user_action_tester.GetActionCount("AppList_HomeButtonPressedTablet"));

  GetEventGenerator()->GestureTapAt(
      home_button->GetBoundsInScreen().CenterPoint());
  ASSERT_EQ(
      1, user_action_tester.GetActionCount("AppList_HomeButtonPressedTablet"));
  EXPECT_TRUE(home_button->IsShowingAppList());
}

TEST_P(LtrRtlShelfViewTest, ShouldHideTooltipTest) {
  // Set a screen size large enough to have space between the home button and
  // app buttons.
  UpdateDisplay("2000x600");
  ShelfID app_button_id = AddAppShortcut();
  ShelfID platform_button_id = AddApp();
  // TODO(manucornet): It should not be necessary to call this manually. The
  // |AddItem| call seems to sometimes be missing some re-layout steps. We
  // should find out what's going on there.
  shelf_view_->UpdateVisibleShelfItemBoundsUnion();
  const HomeButton* home_button =
      GetPrimaryShelf()->navigation_widget()->GetHomeButton();

  // Make sure we're not showing the app list.
  EXPECT_FALSE(home_button->IsShowingAppList())
      << "We should not be showing the app list";

  // The tooltip shouldn't hide if the mouse is on normal buttons.
  for (size_t i = 0; i < test_api_->GetButtonCount(); i++) {
    ShelfAppButton* button = test_api_->GetButton(i);
    if (!button) {
      continue;
    }
    EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
        button->GetMirroredBounds().CenterPoint(), shelf_view_))
        << "ShelfView tries to hide on button " << i;
  }

  // The tooltip should hide if placed in between the home button and the
  // first shelf button.
  const int left = home_button->GetBoundsInScreen().right();
  // Find the first shelf button that's to the right of the home button.
  int right = 0;
  for (size_t i = 0; i < test_api_->GetButtonCount(); ++i) {
    ShelfAppButton* button = test_api_->GetButton(i);
    if (!button) {
      continue;
    }
    right = button->GetBoundsInScreen().x();
    if (right > left) {
      break;
    }
  }

  gfx::Point test_point(left + (right - left) / 2,
                        home_button->GetBoundsInScreen().y());
  views::View::ConvertPointFromScreen(shelf_view_, &test_point);
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(shelf_view_->GetMirroredXInView(test_point.x()),
                 test_point.y()),
      shelf_view_))
      << "Tooltip should hide between home button and first shelf item";

  // The tooltip shouldn't hide if the mouse is in the gap between two buttons.
  gfx::Rect app_button_rect = GetButtonByID(app_button_id)->GetMirroredBounds();
  gfx::Rect platform_button_rect =
      GetButtonByID(platform_button_id)->GetMirroredBounds();
  ASSERT_FALSE(app_button_rect.Intersects(platform_button_rect));
  EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
      gfx::UnionRects(app_button_rect, platform_button_rect).CenterPoint(),
      shelf_view_));

  // The tooltip should hide if it's outside of all buttons.
  gfx::Rect all_area;
  for (size_t i = 0; i < test_api_->GetButtonCount(); i++) {
    ShelfAppButton* button = test_api_->GetButton(i);
    if (!button) {
      continue;
    }

    all_area.Union(button->GetMirroredBounds());
  }
  EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.right() - 1, all_area.bottom() - 1), shelf_view_));
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.right(), all_area.y()), shelf_view_));
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.x() - 1, all_area.y()), shelf_view_));
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.x(), all_area.y() - 1), shelf_view_));
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.x(), all_area.bottom()), shelf_view_));
}

// Test that shelf button tooltips show (except app list) with an open app list.
TEST_P(LtrRtlShelfViewTest, ShouldHideTooltipWithAppListWindowTest) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // The tooltip shouldn't hide if the mouse is on normal buttons.
  for (size_t i = 2; i < test_api_->GetButtonCount(); i++) {
    ShelfAppButton* button = test_api_->GetButton(i);
    if (!button) {
      continue;
    }

    EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
        button->GetMirroredBounds().CenterPoint(), shelf_view_))
        << "ShelfView tries to hide on button " << i;
  }

  // The tooltip should hide on the home button if the app list is visible.
  HomeButton* home_button =
      GetPrimaryShelf()->navigation_widget()->GetHomeButton();
  gfx::Point center_point = home_button->GetBoundsInScreen().CenterPoint();
  views::View::ConvertPointFromScreen(shelf_view_, &center_point);
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(shelf_view_->GetMirroredXInView(center_point.x()),
                 center_point.y()),
      shelf_view_));
}

// Test that by moving the mouse cursor off the button onto the bubble it closes
// the bubble.
TEST_P(LtrRtlShelfViewTest, ShouldHideTooltipWhenHoveringOnTooltip) {
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();
  tooltip_manager->set_timer_delay_for_test(0);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Move the mouse off any item and check that no tooltip is shown.
  generator->MoveMouseTo(gfx::Point(0, 0));
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Move the mouse over the button and check that it is visible.
  views::View* button = shelf_view_->first_visible_button_for_testing();
  gfx::Rect bounds = button->GetBoundsInScreen();
  generator->MoveMouseTo(bounds.CenterPoint());
  // Wait for the timer to go off.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Move the mouse cursor slightly to the right of the item. The tooltip should
  // now close.
  generator->MoveMouseBy(bounds.width() / 2 + 5, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Move back - it should appear again.
  generator->MoveMouseBy(-(bounds.width() / 2 + 5), 0);
  // Make sure there is no delayed close.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Now move the mouse cursor slightly above the item - so that it is over the
  // tooltip bubble. Now it should disappear.
  generator->MoveMouseBy(0, -(bounds.height() / 2 + 5));
  // Wait until the delayed close kicked in.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tooltip_manager->IsVisible());
}

// Checks the rip an item off from left aligned shelf in secondary monitor.
TEST_P(LtrRtlShelfViewTest, CheckRipOffFromLeftShelfAlignmentWithMultiMonitor) {
  UpdateDisplay("800x600,800x600");
  ASSERT_EQ(2U, Shell::GetAllRootWindows().size());

  aura::Window* root_window = Shell::GetAllRootWindows()[1];
  Shelf* secondary_shelf = Shelf::ForWindow(root_window);

  secondary_shelf->SetAlignment(ShelfAlignment::kLeft);
  ASSERT_EQ(ShelfAlignment::kLeft, secondary_shelf->alignment());

  ShelfView* shelf_view_for_secondary =
      secondary_shelf->GetShelfViewForTesting();

  AddAppShortcut();
  ShelfViewTestAPI test_api_for_secondary_shelf_view(shelf_view_for_secondary);
  ShelfAppButton* button = test_api_for_secondary_shelf_view.GetButton(0);

  // Fetch the start point of dragging.
  gfx::Point start_point = button->GetBoundsInScreen().CenterPoint();
  gfx::Point end_point = start_point + gfx::Vector2d(400, 0);
  ::wm::ConvertPointFromScreen(root_window, &start_point);
  ui::test::EventGenerator generator(root_window, start_point);

  // Rip off the browser item.
  generator.PressLeftButton();
  generator.MoveMouseTo(end_point);
  test_api_for_secondary_shelf_view.RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(test_api_for_secondary_shelf_view.IsRippedOffFromShelf());

  // Release the button to prevent crash in test destructor (releasing the
  // button triggers animating shelf to ideal bounds during shell destruction).
  generator.ReleaseLeftButton();
}

// Verifies that Launcher_ButtonPressed_* UMA user actions are recorded when an
// item is selected.
TEST_P(LtrRtlShelfViewTest,
       Launcher_ButtonPressedUserActionsRecordedWhenItemSelected) {
  base::UserActionTester user_action_tester;

  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->ReplaceShelfItemDelegate(
      model_->items()[0].id,
      base::WrapUnique<ShelfItemSelectionTracker>(selection_tracker));

  SimulateClick(0);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("Launcher_ButtonPressed_Mouse"));
}

// Verifies that Launcher_*Task UMA user actions are recorded when an item is
// selected.
TEST_P(LtrRtlShelfViewTest, Launcher_TaskUserActionsRecordedWhenItemSelected) {
  base::UserActionTester user_action_tester;

  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  selection_tracker->set_item_selected_action(SHELF_ACTION_NEW_WINDOW_CREATED);
  model_->ReplaceShelfItemDelegate(
      model_->items()[0].id,
      base::WrapUnique<ShelfItemSelectionTracker>(selection_tracker));

  SimulateClick(0);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Launcher_LaunchTask"));
}

// Verifies that metrics are recorded when an item is minimized and subsequently
// activated.
TEST_P(LtrRtlShelfViewTest,
       VerifyMetricsAreRecordedWhenAnItemIsMinimizedAndActivated) {
  base::HistogramTester histogram_tester;

  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->ReplaceShelfItemDelegate(
      model_->items()[0].id,
      base::WrapUnique<ShelfItemSelectionTracker>(selection_tracker));

  selection_tracker->set_item_selected_action(SHELF_ACTION_WINDOW_MINIMIZED);
  SimulateClick(0);

  selection_tracker->set_item_selected_action(SHELF_ACTION_WINDOW_ACTIVATED);
  SimulateClick(0);

  histogram_tester.ExpectTotalCount(
      kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName, 1);
}

// Verify the animations of the shelf items are as long as expected.
TEST_P(LtrRtlShelfViewTest, TestShelfItemsAnimations) {
  TestShelfObserver observer(shelf_view_->shelf());
  ui::test::EventGenerator* generator = GetEventGenerator();
  ShelfID first_app_id = AddAppShortcut();
  ShelfID second_app_id = AddAppShortcut();

  // Set the animation duration for shelf items.
  test_api_->SetAnimationDuration(base::Milliseconds(100));

  // The shelf items should animate if they are moved within the shelf, either
  // by swapping or if the items need to be rearranged due to an item getting
  // ripped off.
  generator->set_current_screen_location(GetButtonCenter(first_app_id));
  generator->DragMouseTo(GetButtonCenter(second_app_id));
  generator->DragMouseBy(0, 50);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(100, observer.icon_positions_animation_duration().InMilliseconds());

  // The shelf items should not animate when the whole shelf and its contents
  // have to move.
  observer.Reset();
  shelf_view_->shelf()->SetAlignment(ShelfAlignment::kLeft);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, observer.icon_positions_animation_duration().InMilliseconds());

  // The shelf items should animate if we are entering or exiting tablet mode,
  // and the shelf alignment is bottom aligned, and scrollable shelf is not
  // enabled.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  const int64_t id = GetPrimaryDisplay().id();
  shelf_view_->shelf()->SetAlignment(ShelfAlignment::kBottom);
  SetShelfAlignmentPref(prefs, id, ShelfAlignment::kBottom);
  observer.Reset();
  ash::TabletModeControllerTestApi().EnterTabletMode();
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, observer.icon_positions_animation_duration().InMilliseconds());

  observer.Reset();
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, observer.icon_positions_animation_duration().InMilliseconds());

  // The shelf items should not animate if we are entering or exiting tablet
  // mode, and the shelf alignment is not bottom aligned.
  shelf_view_->shelf()->SetAlignment(ShelfAlignment::kLeft);
  SetShelfAlignmentPref(prefs, id, ShelfAlignment::kLeft);
  observer.Reset();
  ash::TabletModeControllerTestApi().EnterTabletMode();
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, observer.icon_positions_animation_duration().InMilliseconds());

  observer.Reset();
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, observer.icon_positions_animation_duration().InMilliseconds());
}

// Tests that the blank shelf view area shows a context menu on right click.
TEST_P(LtrRtlShelfViewTest, ShelfViewShowsContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(shelf_view_->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();
  generator->ReleaseRightButton();

  EXPECT_TRUE(test_api_->CloseMenu());
}

TEST_P(LtrRtlShelfViewTest, TabletModeStartAndEndClosesContextMenu) {
  // Show a context menu on the shelf
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(shelf_view_->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();

  // Start tablet mode, which should close the menu.
  TabletModeControllerTestApi().EnterTabletMode();

  // Attempt to close the menu, which should already be closed.
  EXPECT_FALSE(test_api_->CloseMenu());

  // Show another context menu on the shelf.
  generator->MoveMouseTo(shelf_view_->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();

  // End tablet mode, which should close the menu.
  TabletModeControllerTestApi().LeaveTabletMode();

  // Attempt to close the menu, which should already be closed.
  EXPECT_FALSE(test_api_->CloseMenu());
}

// Tests that ShelfWindowWatcher buttons show a context menu on right click.
TEST_P(LtrRtlShelfViewTest, ShelfWindowWatcherButtonShowsContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  ShelfID shelf_id("123");
  window->SetProperty(kShelfIDKey, shelf_id.Serialize());
  window->SetProperty(kShelfItemTypeKey, static_cast<int32_t>(TYPE_DIALOG));
  ShelfAppButton* button = GetButtonByID(shelf_id);
  ASSERT_TRUE(button);
  generator->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();
  EXPECT_TRUE(test_api_->CloseMenu());
}

// Tests that the drag view is set on left click and not set on right click.
TEST_P(LtrRtlShelfViewTest, ShelfDragViewAndContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  ShelfID shelf_id("123");
  window->SetProperty(kShelfIDKey, shelf_id.Serialize());
  window->SetProperty(kShelfItemTypeKey, static_cast<int32_t>(TYPE_DIALOG));

  // Waits for the bounds animation triggered by window property setting to
  // finish.
  test_api_->RunMessageLoopUntilAnimationsDone();

  ShelfAppButton* button = GetButtonByID(shelf_id);
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

  EXPECT_EQ(0, GetHapticTickEventsCount());
}

// Tests that context menu show is cancelled if item drag starts during context
// menu show (while constructing the item menu model).
TEST_P(LtrRtlShelfViewTest, InProgressItemDragPreventsContextMenuShow) {
  const ShelfID app_id = AddAppShortcut();
  auto item_delegate_owned =
      std::make_unique<AsyncContextMenuShelfItemDelegate>();
  AsyncContextMenuShelfItemDelegate* item_delegate = item_delegate_owned.get();
  model_->ReplaceShelfItemDelegate(app_id, std::move(item_delegate_owned));

  ShelfAppButton* button = GetButtonByID(app_id);
  ASSERT_TRUE(button);

  const gfx::Point location = button->GetBoundsInScreen().CenterPoint();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(location);
  generator->PressTouch();

  // Generate long press, which should start context menu request.
  ui::GestureEventDetails event_details(ui::EventType::kGestureLongPress);
  ui::GestureEvent long_press(location.x(), location.y(), 0,
                              ui::EventTimeForNow(), event_details);
  generator->Dispatch(&long_press);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(button->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());

  // Drag the app icon while context menu callback is pending..
  ASSERT_TRUE(button->FireDragTimerForTest());
  generator->MoveTouchBy(0, -10);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_TRUE(shelf_view_->drag_view());
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_DRAGGING);

  // Return the context menu model.
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
  menu_model->AddItem(203, u"item");
  ASSERT_TRUE(
      item_delegate->RunPendingContextMenuCallback(std::move(menu_model)));

  // The context menu show is expected to be canceled by the item drag.
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_TRUE(shelf_view_->drag_view());
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_DRAGGING);

  // Drag state should be cleared when the drag ends.
  generator->ReleaseTouch();
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(button->state() & ShelfAppButton::STATE_DRAGGING);

  // Another long press starts context menu request.
  generator->set_current_screen_location(location);
  generator->PressTouch();
  ui::GestureEventDetails second_press_event_details(
      ui::EventType::kGestureLongPress);
  ui::GestureEvent second_long_press(location.x(), location.y(), 0,
                                     ui::EventTimeForNow(),
                                     second_press_event_details);
  generator->Dispatch(&second_long_press);
  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());
}

// Tests that context menu show is cancelled if item drag starts and ends during
// context menu show (while constructing the item menu model).
TEST_P(LtrRtlShelfViewTest, CompletedItemDragPreventsContextMenuShow) {
  const ShelfID app_id = AddAppShortcut();
  auto item_delegate_owned =
      std::make_unique<AsyncContextMenuShelfItemDelegate>();
  AsyncContextMenuShelfItemDelegate* item_delegate = item_delegate_owned.get();
  model_->ReplaceShelfItemDelegate(app_id, std::move(item_delegate_owned));

  ShelfAppButton* button = GetButtonByID(app_id);
  ASSERT_TRUE(button);

  const gfx::Point location = button->GetBoundsInScreen().CenterPoint();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(location);
  generator->PressTouch();

  // Generate long press, which should start context menu request.
  ui::GestureEventDetails event_details(ui::EventType::kGestureLongPress);
  ui::GestureEvent long_press(location.x(), location.y(), 0,
                              ui::EventTimeForNow(), event_details);
  generator->Dispatch(&long_press);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(button->state() & ShelfAppButton::STATE_DRAGGING);

  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());

  // Drag the app icon while context menu callback is pending.
  ASSERT_TRUE(button->FireDragTimerForTest());
  generator->MoveTouchBy(0, -10);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_TRUE(shelf_view_->drag_view());
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_DRAGGING);

  // Drag state should be cleared when the drag ends.
  generator->ReleaseTouch();
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(button->state() & ShelfAppButton::STATE_DRAGGING);

  // Return the context menu model.
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
  menu_model->AddItem(203, u"item");
  ASSERT_TRUE(
      item_delegate->RunPendingContextMenuCallback(std::move(menu_model)));

  // The context menu show is expected to be canceled by the item drag, so it
  // should not be shown even though there is no in-progress drag when the
  // context menu model is received.
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(button->state() & ShelfAppButton::STATE_DRAGGING);

  // Another long press starts context menu request.
  generator->set_current_screen_location(location);
  generator->PressTouch();
  ui::GestureEventDetails second_press_event_details(
      ui::EventType::kGestureLongPress);
  ui::GestureEvent second_long_press(location.x(), location.y(), 0,
                                     ui::EventTimeForNow(),
                                     second_press_event_details);
  generator->Dispatch(&second_long_press);
  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());
}

// Tests that shelf items in always shown shelf can be dragged through gesture
// events after context menu is shown.
TEST_P(LtrRtlShelfViewTest, DragAppAfterContextMenuIsShownInAlwaysShownShelf) {
  ASSERT_EQ(SHELF_VISIBLE, GetPrimaryShelf()->GetVisibilityState());
  ui::test::EventGenerator* generator = GetEventGenerator();
  const ShelfID first_app_id = AddAppShortcut();
  const ShelfID second_app_id = AddAppShortcut();
  const int last_index = model_->items().size() - 1;
  ASSERT_GE(last_index, 0);

  ShelfAppButton* button = GetButtonByID(first_app_id);
  ASSERT_TRUE(button);

  auto item_delegate_owned =
      std::make_unique<AsyncContextMenuShelfItemDelegate>();
  AsyncContextMenuShelfItemDelegate* item_delegate = item_delegate_owned.get();
  model_->ReplaceShelfItemDelegate(first_app_id,
                                   std::move(item_delegate_owned));

  const gfx::Point start = GetButtonCenter(first_app_id);
  // Drag the app long enough to ensure the drag can be triggered.
  const gfx::Point end(start.x() + (IsRtlEnabled() ? -100 : 100), start.y());
  generator->set_current_screen_location(start);
  generator->PressTouch();
  ASSERT_TRUE(button->FireDragTimerForTest());
  button->FireRippleActivationTimerForTest();

  // Generate long press, which should start context menu request.
  ui::GestureEventDetails event_details(ui::EventType::kGestureLongPress);
  ui::GestureEvent long_press(start.x(), start.y(), 0, ui::EventTimeForNow(),
                              event_details);
  generator->Dispatch(&long_press);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());

  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_DRAGGING);

  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());

  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
  menu_model->AddItem(203, u"item");
  ASSERT_TRUE(
      item_delegate->RunPendingContextMenuCallback(std::move(menu_model)));

  EXPECT_TRUE(shelf_view_->IsShowingMenu());
  EXPECT_EQ(button, shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_DRAGGING);

  generator->GestureScrollSequence(start, end, base::Milliseconds(100), 3);
  generator->ReleaseTouch();
  EXPECT_EQ(0, GetHapticTickEventsCount());

  // |first_add_id| has been moved to the end of the items in the shelf.
  EXPECT_EQ(first_app_id, model_->items()[last_index].id);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(button->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_FALSE(item_delegate->HasPendingContextMenuCallback());
}

// Tests that shelf items in AUTO_HIDE_SHOWN shelf can be dragged through
// gesture events after context menu is shown.
TEST_P(LtrRtlShelfViewTest, DragAppAfterContextMenuIsShownInAutoHideShelf) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  const ShelfID first_app_id = AddAppShortcut();
  const ShelfID second_app_id = AddAppShortcut();
  const int last_index = model_->items().size() - 1;

  Shelf* shelf = GetPrimaryShelf();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->Show();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  shelf->shelf_widget()->GetFocusCycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  ShelfAppButton* button = GetButtonByID(first_app_id);
  ASSERT_TRUE(button);

  auto item_delegate_owned =
      std::make_unique<AsyncContextMenuShelfItemDelegate>();
  AsyncContextMenuShelfItemDelegate* item_delegate = item_delegate_owned.get();
  model_->ReplaceShelfItemDelegate(first_app_id,
                                   std::move(item_delegate_owned));

  const gfx::Point start = GetButtonCenter(first_app_id);
  // Drag the app long enough to ensure the drag can be triggered.
  const gfx::Point end(start.x() + (IsRtlEnabled() ? -100 : 100), start.y());
  generator->set_current_screen_location(start);
  generator->PressTouch();
  ASSERT_TRUE(button->FireDragTimerForTest());
  button->FireRippleActivationTimerForTest();

  // Generate long press, which should start context menu request.
  ui::GestureEventDetails event_details(ui::EventType::kGestureLongPress);
  ui::GestureEvent long_press(start.x(), start.y(), 0, ui::EventTimeForNow(),
                              event_details);
  generator->Dispatch(&long_press);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());

  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_DRAGGING);

  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());

  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
  menu_model->AddItem(203, u"item");
  ASSERT_TRUE(
      item_delegate->RunPendingContextMenuCallback(std::move(menu_model)));

  EXPECT_TRUE(shelf_view_->IsShowingMenu());
  EXPECT_EQ(button, shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_DRAGGING);

  generator->GestureScrollSequence(start, end, base::Milliseconds(100), 3);
  generator->ReleaseTouch();
  EXPECT_EQ(0, GetHapticTickEventsCount());

  // |first_add_id| has been moved to the end of the items in the shelf.
  EXPECT_EQ(first_app_id, model_->items()[last_index].id);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(button->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_FALSE(item_delegate->HasPendingContextMenuCallback());
}

// Tests that the app button returns to normal state after context menu is
// hidden and touch is released, even if another touch point is added while the
// context menu is shown.
TEST_P(LtrRtlShelfViewTest,
       DragStateIsClearedIfAnotherTouchIsAddedWithContextMenu) {
  ASSERT_EQ(SHELF_VISIBLE, GetPrimaryShelf()->GetVisibilityState());
  ui::test::EventGenerator* generator = GetEventGenerator();
  const ShelfID app_id = AddAppShortcut();

  ShelfAppButton* button = GetButtonByID(app_id);
  ASSERT_TRUE(button);

  auto item_delegate_owned =
      std::make_unique<AsyncContextMenuShelfItemDelegate>();
  AsyncContextMenuShelfItemDelegate* item_delegate = item_delegate_owned.get();
  model_->ReplaceShelfItemDelegate(app_id, std::move(item_delegate_owned));

  const gfx::Point location = GetButtonCenter(app_id);
  generator->set_current_screen_location(location);
  generator->PressTouch();
  ASSERT_TRUE(button->FireDragTimerForTest());
  button->FireRippleActivationTimerForTest();

  // Generate long press, which should start context menu request.
  ui::GestureEventDetails event_details(ui::EventType::kGestureLongPress);
  ui::GestureEvent long_press(location.x(), location.y(), 0,
                              ui::EventTimeForNow(), event_details);
  generator->Dispatch(&long_press);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_DRAGGING);

  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());

  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
  menu_model->AddItem(203, u"item");
  ASSERT_TRUE(
      item_delegate->RunPendingContextMenuCallback(std::move(menu_model)));

  EXPECT_TRUE(shelf_view_->IsShowingMenu());
  EXPECT_EQ(button, shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_DRAGGING);

  // Add and release touch.
  generator->PressTouchId(2, gfx::Point(10, 10));
  generator->ReleaseTouchId(2);

  EXPECT_TRUE(shelf_view_->IsShowingMenu());
  EXPECT_EQ(button, shelf_view_->GetShelfItemViewWithContextMenu());

  // Another tap to hide the context menu.
  generator->PressTouchId(2, gfx::Point(10, 10));
  generator->ReleaseTouchId(2);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_FALSE(button->state() & ShelfAppButton::STATE_DRAGGING);

  // Releasing the original touch should not show another menu.
  generator->ReleaseTouch();
  EXPECT_EQ(0, GetHapticTickEventsCount());

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(button->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_FALSE(item_delegate->HasPendingContextMenuCallback());
}

// Tests that app button context menu remains shown if touch moves slightly
// outside the button bounds while the menu is shown (not enough to initiate
// drag gesture).
TEST_P(LtrRtlShelfViewTest,
       AppContextMenuRemainsShowingAfterTouchSlightlyMovesOutsideTheButton) {
  ASSERT_EQ(SHELF_VISIBLE, GetPrimaryShelf()->GetVisibilityState());
  ui::test::EventGenerator* generator = GetEventGenerator();
  const ShelfID app_id = AddAppShortcut();

  ShelfAppButton* button = GetButtonByID(app_id);
  ASSERT_TRUE(button);

  auto item_delegate_owned =
      std::make_unique<AsyncContextMenuShelfItemDelegate>();
  AsyncContextMenuShelfItemDelegate* item_delegate = item_delegate_owned.get();
  model_->ReplaceShelfItemDelegate(app_id, std::move(item_delegate_owned));

  // Start near the button edge.
  const gfx::Point location =
      button->GetBoundsInScreen().bottom_right() + gfx::Vector2d(-1, -1);
  generator->set_current_screen_location(location);
  generator->PressTouch();
  ASSERT_TRUE(button->FireDragTimerForTest());
  button->FireRippleActivationTimerForTest();

  // Generate long press, which should start context menu request.
  ui::GestureEventDetails event_details(ui::EventType::kGestureLongPress);
  ui::GestureEvent long_press(location.x(), location.y(), 0,
                              ui::EventTimeForNow(), event_details);
  generator->Dispatch(&long_press);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_DRAGGING);

  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());

  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
  menu_model->AddItem(203, u"item");
  ASSERT_TRUE(
      item_delegate->RunPendingContextMenuCallback(std::move(menu_model)));

  EXPECT_TRUE(shelf_view_->IsShowingMenu());
  EXPECT_EQ(button, shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_DRAGGING);

  // Move touch outside the button bounds using small distance - not enough to
  // start a drag operation that would hide the context menu.
  generator->MoveTouch(button->GetBoundsInScreen().bottom_right() +
                       gfx::Vector2d(1, -1));

  EXPECT_TRUE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_EQ(button, shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_DRAGGING);

  // Release touch - button drag state should be cleared, and the context menu
  // remain visible.
  generator->ReleaseTouch();

  EXPECT_TRUE(shelf_view_->IsShowingMenu());
  EXPECT_TRUE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(button->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_FALSE(item_delegate->HasPendingContextMenuCallback());

  // Tap to close the context menu.
  generator->GestureTapAt(gfx::Point(10, 10));

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->GetShelfItemViewWithContextMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(button->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_FALSE(item_delegate->HasPendingContextMenuCallback());
}

// Tests that the home button does shows a context menu on right click.
TEST_P(LtrRtlShelfViewTest, HomeButtonDoesShowContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  const HomeButton* home_button =
      GetPrimaryShelf()->navigation_widget()->GetHomeButton();
  generator->MoveMouseTo(home_button->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();
  EXPECT_TRUE(test_api_->CloseMenu());
}

void ExpectWithinOnePixel(int a, int b) {
  EXPECT_TRUE(abs(a - b) <= 1) << "Values " << a << " and " << b
                               << " should have a difference no greater than 1";
}

TEST_P(LtrRtlShelfViewTest, IconCenteringTest) {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  const int screen_width = display.bounds().width();
  const int screen_center = screen_width / 2;

  // Show the IME panel, to introduce for asymettry with a larger status area.
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);

  // At the start, we have exactly one app icon for the browser. That should
  // be centered on the screen.
  const ShelfAppButton* button1 = GetButtonByID(model_->items()[0].id);
  ExpectWithinOnePixel(screen_center,
                       button1->GetBoundsInScreen().CenterPoint().x());
  // Also check that the distance between the icon edge and the screen edge is
  // the same on both sides.
  ExpectWithinOnePixel(button1->GetBoundsInScreen().x(),
                       screen_width - button1->GetBoundsInScreen().right());

  const int apps_that_can_easily_fit_at_center_of_screen = 3;
  std::vector<ShelfAppButton*> app_buttons;
  // Start with just the browser app button.
  app_buttons.push_back(GetButtonByID(model_->items()[0].id));
  int n_buttons = 1;

  // Now repeat the same process by adding apps until they can't fit at the
  // center of the screen.
  for (int i = 1; i < apps_that_can_easily_fit_at_center_of_screen; ++i) {
    // Add a new app and add its button to our list.
    app_buttons.push_back(GetButtonByID(AddApp()));
    n_buttons = app_buttons.size();
    if (n_buttons % 2 == 1) {
      // Odd number of apps. Check that the middle app is exactly at the center
      // of the screen.
      ExpectWithinOnePixel(
          screen_center,
          app_buttons[n_buttons / 2]->GetBoundsInScreen().CenterPoint().x());
    }
    // Also check that the first icon is at the same distance from the left
    // screen edge as the last icon is from the right screen edge.
    ExpectWithinOnePixel(
        app_buttons[0]->GetBoundsInScreen().x(),
        screen_width - app_buttons[n_buttons - 1]->GetBoundsInScreen().right());
  }
}

TEST_P(LtrRtlShelfViewTest, FirstAndLastVisibleIndex) {
  // At the start, the only visible app on the shelf is the browser app button
  // (index 0).
  ASSERT_EQ(1u, shelf_view_->visible_views_indices().size());
  EXPECT_EQ(0u, shelf_view_->visible_views_indices()[0]);
  // By enabling tablet mode, the back button (index 0) should become visible,
  // but that does not change the first and last visible indices.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_EQ(1u, shelf_view_->visible_views_indices().size());
  EXPECT_EQ(0u, shelf_view_->visible_views_indices()[0]);
  // Turn tablet mode off again.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  ASSERT_EQ(1u, shelf_view_->visible_views_indices().size());
  EXPECT_EQ(0u, shelf_view_->visible_views_indices()[0]);
}

TEST_P(LtrRtlShelfViewTest, ReplacingDelegateCancelsContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  ShelfID app_button_id = AddAppShortcut();
  generator->MoveMouseTo(GetButtonCenter(GetButtonByID(app_button_id)));

  // Right click should open the context menu.
  generator->PressRightButton();
  generator->ReleaseRightButton();
  EXPECT_TRUE(shelf_view_->IsShowingMenu());

  // Replacing the item delegate should close the context menu.
  model_->ReplaceShelfItemDelegate(
      app_button_id, std::make_unique<ShelfItemSelectionTracker>());
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
}

// Verifies that shelf is shown with the app list in fullscreen mode, and that
// shelf app buttons are clickable.
TEST_P(LtrRtlShelfViewTest, ClickItemInFullscreen) {
  ShelfID app_button_id = AddAppShortcut();
  auto selection_tracker_owned = std::make_unique<ShelfItemSelectionTracker>();
  ShelfItemSelectionTracker* selection_tracker = selection_tracker_owned.get();
  model_->ReplaceShelfItemDelegate(app_button_id,
                                   std::move(selection_tracker_owned));

  // Create a fullscreen widget.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->SetFullscreen(true);
  WindowState* window_state = WindowState::Get(widget->GetNativeWindow());
  window_state->SetHideShelfWhenFullscreen(true);

  Shelf* const shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  // Show app list, which will bring the shelf up.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Verify that clicking a shelf button activates it.
  EXPECT_EQ(0u, selection_tracker->item_selected_count());
  const ShelfAppButton* shelf_button = GetButtonByID(app_button_id);
  GetEventGenerator()->MoveMouseTo(
      shelf_button->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ(1u, selection_tracker->item_selected_count());

  // Shelf gets hidden when the app list is dismissed.
  GetAppListTestHelper()->DismissAndRunLoop();
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
  EXPECT_EQ(0, GetHapticTickEventsCount());
}

// Verifies that shelf is shown with the app list in fullscreen mode, and that
// shelf app buttons are tappable.
TEST_P(LtrRtlShelfViewTest, TapInFullscreen) {
  ShelfID app_button_id = AddAppShortcut();
  auto selection_tracker_owned = std::make_unique<ShelfItemSelectionTracker>();
  ShelfItemSelectionTracker* selection_tracker = selection_tracker_owned.get();
  model_->ReplaceShelfItemDelegate(app_button_id,
                                   std::move(selection_tracker_owned));

  // Create a fullscreen widget.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->SetFullscreen(true);
  WindowState* window_state = WindowState::Get(widget->GetNativeWindow());
  window_state->SetHideShelfWhenFullscreen(true);

  Shelf* const shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  // Show app list, which will bring the shelf up.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Verify that tapping a shelf button activates it.
  EXPECT_EQ(0u, selection_tracker->item_selected_count());
  const ShelfAppButton* shelf_button = GetButtonByID(app_button_id);
  GetEventGenerator()->GestureTapAt(
      shelf_button->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, selection_tracker->item_selected_count());

  // Shelf gets hidden when the app list is dismissed.
  GetAppListTestHelper()->DismissAndRunLoop();
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
  EXPECT_EQ(0, GetHapticTickEventsCount());
}

// Test class that tests both context and application menus.
class ShelfViewMenuTest : public ShelfViewTest,
                          public testing::WithParamInterface<bool> {
 public:
  ShelfViewMenuTest() = default;

  ShelfViewMenuTest(const ShelfViewMenuTest&) = delete;
  ShelfViewMenuTest& operator=(const ShelfViewMenuTest&) = delete;

  ~ShelfViewMenuTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(All, ShelfViewMenuTest, testing::Bool());

// Tests that menu anchor points are aligned with the shelf button bounds.
TEST_P(ShelfViewMenuTest, ShelfViewMenuAnchorPoint) {
  const ShelfAppButton* shelf_button = GetButtonByID(AddApp());
  const bool context_menu = GetParam();
  EXPECT_EQ(ShelfAlignment::kBottom, GetPrimaryShelf()->alignment());

  // Test for bottom shelf.
  EXPECT_EQ(
      shelf_button->GetBoundsInScreen().y(),
      test_api_->GetMenuAnchorRect(*shelf_button, gfx::Point(), context_menu)
          .y());

  // Test for left shelf.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);

  EXPECT_EQ(
      shelf_button->GetBoundsInScreen().x(),
      test_api_->GetMenuAnchorRect(*shelf_button, gfx::Point(), context_menu)
          .x());

  // Test for right shelf.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);

  EXPECT_EQ(
      shelf_button->GetBoundsInScreen().x(),
      test_api_->GetMenuAnchorRect(*shelf_button, gfx::Point(), context_menu)
          .x());
}

// Tests that an item has a notification badge indicator when the notification
// is added and removed.
TEST_F(ShelfViewTest, ItemHasCorrectNotificationBadgeIndicator) {
  const ShelfID item_id = AddApp();
  const ShelfAppButton* shelf_app_button = GetButtonByID(item_id);

  EXPECT_FALSE(GetItemByID(item_id).has_notification);
  EXPECT_FALSE(shelf_app_button->state() & ShelfAppButton::STATE_NOTIFICATION);

  // Add a notification for the new shelf item.
  model_->UpdateItemNotification(item_id.app_id, true /* has_badge */);

  EXPECT_TRUE(GetItemByID(item_id).has_notification);
  EXPECT_TRUE(shelf_app_button->state() & ShelfAppButton::STATE_NOTIFICATION);

  // Remove notification.
  model_->UpdateItemNotification(item_id.app_id, false /* has_badge */);

  EXPECT_FALSE(GetItemByID(item_id).has_notification);
  EXPECT_FALSE(shelf_app_button->state() & ShelfAppButton::STATE_NOTIFICATION);
}

TEST_F(ShelfViewTest, TapOnItemDuringFadeOut) {
  const ShelfID test_item_id = AddApp();

  ash::TabletModeControllerTestApi().EnterTabletMode();

  views::View* const test_item_button = GetButtonByID(test_item_id);
  ASSERT_TRUE(test_item_button);
  const gfx::Point test_item_location =
      test_item_button->GetBoundsInScreen().CenterPoint();

  // Enable animations, as the test verifies behavior while a fade out animation
  // is in progress.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Simulate test app getting uninstalled.
  model_->RemoveItemAt(model_->ItemIndexByID(test_item_id));

  // Tap on the removed item bounds (which will remain in place during fadeout
  // animation).
  GetEventGenerator()->GestureTapAt(test_item_location);

  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyShelfItemBoundsAreValid();
}

TEST_F(ShelfViewTest, SwipeOnItemDuringFadeOut) {
  const ShelfID test_item_id = AddApp();

  ash::TabletModeControllerTestApi().EnterTabletMode();

  views::View* const test_item_button = GetButtonByID(test_item_id);
  ASSERT_TRUE(test_item_button);
  const gfx::Point test_item_location =
      test_item_button->GetBoundsInScreen().CenterPoint();

  // Enable animations, as the test verifies behavior while a fade out animation
  // is in progress.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Simulate test app getting uninstalled.
  model_->RemoveItemAt(model_->ItemIndexByID(test_item_id));

  // Swipe from the removed item bounds (which will remain in place during
  // fadeout animation).
  GetEventGenerator()->GestureScrollSequence(
      test_item_location, test_item_location + gfx::Vector2d(0, -50),
      base::Milliseconds(100), 3);

  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyShelfItemBoundsAreValid();
}

class GhostImageShelfViewTest : public ShelfViewTest {
 public:
  GhostImageShelfViewTest() = default;

  GhostImageShelfViewTest(const GhostImageShelfViewTest&) = delete;
  GhostImageShelfViewTest& operator=(const GhostImageShelfViewTest&) = delete;

  ~GhostImageShelfViewTest() override = default;

  void StartDrag(ShelfAppButton* dragged) {
    ASSERT_TRUE(dragged);
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->set_current_screen_location(
        dragged->GetBoundsInScreen().CenterPoint());
    generator->PressTouch();
    ASSERT_TRUE(dragged->FireDragTimerForTest());
  }
};

// Tests that the ghost image shows during a drag operation.
TEST_F(GhostImageShelfViewTest, ShowGhostImageOnDrag) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);
  ShelfAppButton* first_app = GetButtonByID(id_map[0].first);

  StartDrag(first_app);
  EXPECT_EQ(0, GetHapticTickEventsCount());

  EXPECT_TRUE(first_app->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(shelf_view_->current_ghost_view_index().has_value());

  ShelfID second_app_id = id_map[1].first;
  GetEventGenerator()->MoveTouch(GetButtonCenter(second_app_id));

  EXPECT_TRUE(first_app->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_TRUE(shelf_view_->drag_view());
  EXPECT_EQ(1u, shelf_view_->current_ghost_view_index());

  GetEventGenerator()->ReleaseTouch();
  EXPECT_EQ(0, GetHapticTickEventsCount());

  EXPECT_FALSE(first_app->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(shelf_view_->current_ghost_view_index().has_value());
}

// Tests that the ghost image is removed if the app is dragged outide of the
// bounds of the shelf.
TEST_F(GhostImageShelfViewTest, RemoveGhostImageForRipOffDrag) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);
  ShelfAppButton* first_app = GetButtonByID(id_map[0].first);

  StartDrag(first_app);
  EXPECT_EQ(0, GetHapticTickEventsCount());

  EXPECT_TRUE(first_app->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(shelf_view_->current_ghost_view_index().has_value());

  ShelfID second_app_id = id_map[1].first;
  GetEventGenerator()->MoveTouch(GetButtonCenter(second_app_id));

  EXPECT_TRUE(first_app->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_TRUE(shelf_view_->drag_view());
  EXPECT_EQ(1u, shelf_view_->current_ghost_view_index());

  // The rip off threshold. Taken from |kRipOffDistance| in shelf_view.cc.
  constexpr int kRipOffDistance = 48;
  // Drag off the shelf to trigger rip off drag.
  GetEventGenerator()->MoveTouch(shelf_view_->GetBoundsInScreen().top_center());
  GetEventGenerator()->MoveTouchBy(0, -kRipOffDistance - 1);

  EXPECT_TRUE(first_app->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_TRUE(shelf_view_->drag_view());
  EXPECT_FALSE(shelf_view_->current_ghost_view_index().has_value());

  GetEventGenerator()->ReleaseTouch();
  EXPECT_EQ(0, GetHapticTickEventsCount());

  EXPECT_FALSE(first_app->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(shelf_view_->current_ghost_view_index().has_value());
}

// Tests that the ghost image is reinserted if the app is dragged within the
// bounds of the shelf after a rip off.
TEST_F(GhostImageShelfViewTest, ReinsertGhostImageAfterRipOffDrag) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);
  ShelfAppButton* first_app = GetButtonByID(id_map[0].first);

  StartDrag(first_app);
  EXPECT_EQ(0, GetHapticTickEventsCount());

  EXPECT_TRUE(first_app->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(shelf_view_->current_ghost_view_index().has_value());

  ShelfID second_app_id = id_map[1].first;
  GetEventGenerator()->MoveTouch(GetButtonCenter(second_app_id));

  EXPECT_TRUE(first_app->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_TRUE(shelf_view_->drag_view());
  EXPECT_EQ(1u, shelf_view_->current_ghost_view_index());

  // The rip off threshold. Taken from |kRipOffDistance| in shelf_view.cc.
  constexpr int kRipOffDistance = 48;
  // Drag off the shelf to trigger rip off drag.
  GetEventGenerator()->MoveTouch(shelf_view_->GetBoundsInScreen().top_center());
  GetEventGenerator()->MoveTouchBy(0, -kRipOffDistance - 1);

  EXPECT_TRUE(first_app->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_TRUE(shelf_view_->drag_view());
  EXPECT_FALSE(shelf_view_->current_ghost_view_index().has_value());

  GetEventGenerator()->MoveTouch(GetButtonCenter(second_app_id));

  EXPECT_TRUE(first_app->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_TRUE(shelf_view_->drag_view());
  EXPECT_EQ(1u, shelf_view_->current_ghost_view_index());

  GetEventGenerator()->ReleaseTouch();
  EXPECT_EQ(0, GetHapticTickEventsCount());

  EXPECT_FALSE(first_app->state() & ShelfAppButton::STATE_DRAGGING);
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_FALSE(shelf_view_->current_ghost_view_index().has_value());
}

class ShelfViewVisibleBoundsTest : public ShelfViewTest,
                                   public testing::WithParamInterface<bool> {
 public:
  ShelfViewVisibleBoundsTest() : scoped_locale_(GetParam() ? "he" : "") {}

  ShelfViewVisibleBoundsTest(const ShelfViewVisibleBoundsTest&) = delete;
  ShelfViewVisibleBoundsTest& operator=(const ShelfViewVisibleBoundsTest&) =
      delete;

  void CheckAllItemsAreInBounds() {
    gfx::Rect visible_bounds = shelf_view_->GetVisibleItemsBoundsInScreen();
    gfx::Rect shelf_bounds = shelf_view_->GetBoundsInScreen();
    EXPECT_TRUE(shelf_bounds.Contains(visible_bounds));
    for (size_t i = 0; i < test_api_->GetButtonCount(); ++i) {
      if (ShelfAppButton* button = test_api_->GetButton(i)) {
        if (button->GetVisible()) {
          EXPECT_TRUE(visible_bounds.Contains(button->GetBoundsInScreen()));
        }
      }
    }
  }

 private:
  // Restores locale to the default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;
};

TEST_P(ShelfViewVisibleBoundsTest, ItemsAreInBounds) {
  // Adding elements leaving some empty space.
  for (int i = 0; i < 3; i++) {
    AddAppShortcut();
  }
  test_api_->RunMessageLoopUntilAnimationsDone();
  CheckAllItemsAreInBounds();
}

INSTANTIATE_TEST_SUITE_P(LtrRtl, ShelfViewVisibleBoundsTest, testing::Bool());

namespace {

// An InkDrop implementation that wraps another InkDrop instance to keep track
// of state changes requested on it. Note that this will only track transitions
// routed through AnimateToState() and not the ones performed directly on the
// ripple inside the contained |ink_drop|.
class InkDropSpy : public views::InkDrop {
 public:
  explicit InkDropSpy(std::unique_ptr<views::InkDrop> ink_drop)
      : ink_drop_(std::move(ink_drop)) {}

  InkDropSpy(const InkDropSpy&) = delete;
  InkDropSpy& operator=(const InkDropSpy&) = delete;

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
  void HostViewThemeChanged() override { ink_drop_->HostViewThemeChanged(); }
  void HostTransformChanged(const gfx::Transform& new_transform) override {
    ink_drop_->HostTransformChanged(new_transform);
  }
  views::InkDropState GetTargetInkDropState() const override {
    return ink_drop_->GetTargetInkDropState();
  }
  void AnimateToState(views::InkDropState ink_drop_state) override {
    requested_states_.push_back(ink_drop_state);
    ink_drop_->AnimateToState(ink_drop_state);
  }

  void SetHoverHighlightFadeDuration(base::TimeDelta duration) override {
    ink_drop_->SetHoverHighlightFadeDuration(duration);
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
};

// A ShelfItemDelegate that returns a menu for the shelf item.
class ListMenuShelfItemDelegate : public ShelfItemDelegate {
 public:
  ListMenuShelfItemDelegate() : ShelfItemDelegate(ShelfID()) {}

  ListMenuShelfItemDelegate(const ListMenuShelfItemDelegate&) = delete;
  ListMenuShelfItemDelegate& operator=(const ListMenuShelfItemDelegate&) =
      delete;

  ~ListMenuShelfItemDelegate() override = default;

 private:
  // ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override {
    // Two items are needed to show a menu; the data in the items is not tested.
    std::move(callback).Run(SHELF_ACTION_NONE, {{}, {}});
  }
  void ExecuteCommand(bool, int64_t, int32_t, int64_t) override {}
  void Close() override {}
};

}  // namespace

// Test fixture for testing material design ink drop ripples on shelf.
class ShelfViewInkDropTest : public ShelfViewTest {
 public:
  ShelfViewInkDropTest() = default;
  ShelfViewInkDropTest(const ShelfViewInkDropTest&) = delete;
  ShelfViewInkDropTest& operator=(const ShelfViewInkDropTest&) = delete;
  ~ShelfViewInkDropTest() override = default;

 protected:
  void InitHomeButtonInkDrop() {
    home_button_ = GetPrimaryShelf()->navigation_widget()->GetHomeButton();

    auto home_button_ink_drop = std::make_unique<InkDropSpy>(
        views::InkDrop::CreateInkDropWithoutAutoHighlight(
            views::InkDrop::Get(home_button_)));
    home_button_ink_drop_ = home_button_ink_drop.get();
    views::test::InkDropHostTestApi(views::InkDrop::Get(home_button_))
        .SetInkDrop(std::move(home_button_ink_drop), false);
  }

  void InitBrowserButtonInkDrop() {
    browser_button_ = test_api_->GetButton(0);

    auto ink_drop_impl = std::make_unique<views::InkDropImpl>(
        views::InkDrop::Get(browser_button_), browser_button_->size(),
        views::InkDropImpl::AutoHighlightMode::NONE);
    browser_button_ink_drop_impl_ = ink_drop_impl.get();

    auto browser_button_ink_drop =
        std::make_unique<InkDropSpy>(std::move(ink_drop_impl));
    browser_button_ink_drop_ = browser_button_ink_drop.get();
    views::test::InkDropHostTestApi(views::InkDrop::Get(browser_button_))
        .SetInkDrop(std::move(browser_button_ink_drop));
  }

  raw_ptr<HomeButton, DanglingUntriaged> home_button_ = nullptr;
  raw_ptr<InkDropSpy, DanglingUntriaged> home_button_ink_drop_ = nullptr;
  raw_ptr<ShelfAppButton, DanglingUntriaged> browser_button_ = nullptr;
  raw_ptr<InkDropSpy, DanglingUntriaged> browser_button_ink_drop_ = nullptr;
  raw_ptr<views::InkDropImpl, DanglingUntriaged> browser_button_ink_drop_impl_ =
      nullptr;
};

// Tests that changing visibility of the app list transitions home button's
// ink drop states correctly.
TEST_F(ShelfViewInkDropTest, HomeButtonWhenVisibilityChanges) {
  InitHomeButtonInkDrop();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());

  GetAppListTestHelper()->DismissAndRunLoop();

  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());
}

// Tests that when the app list is hidden, mouse press on the home button,
// which shows the app list, transitions ink drop states correctly. Also, tests
// that mouse drag and mouse release does not affect the ink drop state.
TEST_F(ShelfViewInkDropTest, HomeButtonMouseEventsWhenHidden) {
  InitHomeButtonInkDrop();

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(home_button_->GetBoundsInScreen().CenterPoint());

  // Mouse press on the button, which shows the app list, should end up in the
  // activated state.
  generator->PressLeftButton();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING,
                          views::InkDropState::ACTION_TRIGGERED));

  // Dragging mouse out and back and releasing the button should not change the
  // ink drop state.
  generator->MoveMouseBy(home_button_->width(), 0);
  generator->MoveMouseBy(-home_button_->width(), 0);
  generator->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());
}

// Tests that when the app list is visible, mouse press on the home button,
// which dismisses the app list, transitions ink drop states correctly. Also,
// tests that mouse drag and mouse release does not affect the ink drop state.
TEST_F(ShelfViewInkDropTest, HomeButtonMouseEventsWhenVisible) {
  InitHomeButtonInkDrop();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());

  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());

  // Mouse press on the button, which dismisses the app list, should end up in
  // the hidden state.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(home_button_->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());

  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING,
                          views::InkDropState::ACTION_TRIGGERED));

  // Dragging mouse out and back and releasing the button should not change the
  // ink drop state.
  generator->MoveMouseBy(home_button_->width(), 0);
  generator->MoveMouseBy(-home_button_->width(), 0);
  generator->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());
}

// Tests that when the app list is hidden, tapping on the home button
// transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, HomeButtonGestureTapWhenHidden) {
  InitHomeButtonInkDrop();

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(home_button_->GetBoundsInScreen().CenterPoint());

  // Touch press on the button should end up in the pending state.
  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());

  // Touch release on the button, which shows the app list, should end up in the
  // activated state.
  generator->ReleaseTouch();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_TRIGGERED));
}

// Tests that when the app list is visible, tapping on the home button
// transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, HomeButtonGestureTapWhenVisible) {
  InitHomeButtonInkDrop();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());

  // Touch press and release on the button, which dismisses the app list, should
  // end up in the hidden state.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(home_button_->GetBoundsInScreen().CenterPoint());
  generator->PressTouch();
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_TRIGGERED));
}

// Tests that when the app list is hidden, tapping down on the home button
// and dragging the touch point transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, HomeButtonGestureTapDragWhenHidden) {
  InitHomeButtonInkDrop();

  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Point touch_location = home_button_->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(touch_location);

  // Touch press on the button should end up in the pending state.
  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());

  // Dragging the touch point should hide the pending ink drop.
  touch_location.Offset(home_button_->width(), 0);
  generator->MoveTouch(touch_location);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());

  // Touch release should not change the ink drop state.
  generator->ReleaseTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());
}

// Tests that when the app list is visible, tapping down on the home button
// and dragging the touch point will not change ink drop states.
TEST_F(ShelfViewInkDropTest, HomeButtonGestureTapDragWhenVisible) {
  InitHomeButtonInkDrop();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());

  // Touch press on the button, dragging the touch point, and releasing, which
  // will not dismisses the app list, should end up in the |ACTIVATED| state.
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Point touch_location = home_button_->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(touch_location);

  // Touch press on the button should not change the ink drop state.
  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());

  // Dragging the touch point should not hide the pending ink drop.
  touch_location.Offset(home_button_->width(), 0);
  generator->MoveTouch(touch_location);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());

  // Touch release should not change the ink drop state.
  generator->ReleaseTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());
}

// Tests that clicking on a shelf item that does not show a menu transitions ink
// drop states correctly.
TEST_F(ShelfViewInkDropTest, ShelfButtonWithoutMenuPressRelease) {
  InitBrowserButtonInkDrop();

  views::Button* button = browser_button_;
  gfx::Point mouse_location = button->GetLocalBounds().CenterPoint();

  ui::MouseEvent press_event(ui::EventType::kMousePressed, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  ui::MouseEvent release_event(ui::EventType::kMouseReleased, mouse_location,
                               mouse_location, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_TRIGGERED));
}

// Verifies the shelf view's behavior when opening an empty context menu.
TEST_F(ShelfViewInkDropTest, ShowEmptyShelfAppContextMenu) {
  InitBrowserButtonInkDrop();

  // Set the shelf item delegate to generate an empty context menu. Note that if
  // we do not specify the shelf item delegate, a default context menu which is
  // non-empty is created.
  const int browser_shortcut_index =
      ShelfModel::Get()->GetItemIndexForType(TYPE_BROWSER_SHORTCUT);
  model_->ReplaceShelfItemDelegate(model_->items()[browser_shortcut_index].id,
                                   std::make_unique<EmptyContextMenuBuilder>());

  // Right mouse click at the browser button.
  const gfx::Rect button_bounds_in_screen =
      browser_button_->GetBoundsInScreen();
  const gfx::Point button_bounds_center = button_bounds_in_screen.CenterPoint();
  GetEventGenerator()->MoveMouseTo(button_bounds_center);
  GetEventGenerator()->ClickRightButton();

  // Verify that the context menu does not show and the inkdrop is hidden.
  auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
  EXPECT_FALSE(shelf_view->IsShowingMenu());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());

  // Press a point outside of the browser button. Then move the mouse without
  // release.
  gfx::Point press_point(button_bounds_in_screen.right() + 5,
                         button_bounds_center.y());
  GetEventGenerator()->MoveMouseTo(press_point);
  GetEventGenerator()->PressLeftButton();
  press_point.Offset(/*x_delta=*/0, /*y_delta=*/-10);
  GetEventGenerator()->MoveMouseTo(press_point);

  // Verify that the browser button is not the mouse handler, which means that
  // the browser button is not under drag.
  EXPECT_FALSE(shelf_view->IsDraggedView(browser_button_));
}

// Tests that dragging outside of a shelf item transitions ink drop states
// correctly.
TEST_F(ShelfViewInkDropTest, ShelfButtonWithoutMenuPressDragReleaseOutside) {
  InitBrowserButtonInkDrop();

  views::Button* button = browser_button_;
  gfx::Point mouse_location = button->GetLocalBounds().CenterPoint();

  ui::MouseEvent press_event(ui::EventType::kMousePressed, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  mouse_location.Offset(test_api_->GetMinimumDragDistance() / 2, 0);
  ui::MouseEvent drag_event_small(ui::EventType::kMouseDragged, mouse_location,
                                  mouse_location, ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event_small);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  mouse_location.Offset(test_api_->GetMinimumDragDistance(), 0);
  ui::MouseEvent drag_event_large(ui::EventType::kMouseDragged, mouse_location,
                                  mouse_location, ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event_large);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  ui::MouseEvent release_event(ui::EventType::kMouseReleased, mouse_location,
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

  ui::MouseEvent press_event(ui::EventType::kMousePressed, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  mouse_location.Offset(test_api_->GetMinimumDragDistance() * 2, 0);
  ui::MouseEvent drag_event_outside(
      ui::EventType::kMouseDragged, mouse_location, mouse_location,
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event_outside);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  mouse_location.Offset(-test_api_->GetMinimumDragDistance() * 2, 0);
  ui::MouseEvent drag_event_inside(ui::EventType::kMouseDragged, mouse_location,
                                   mouse_location, ui::EventTimeForNow(),
                                   ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event_inside);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  ui::MouseEvent release_event(ui::EventType::kMouseReleased, mouse_location,
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
  model_->ReplaceShelfItemDelegate(
      model_->items()[0].id, std::make_unique<ListMenuShelfItemDelegate>());

  views::Button* button = browser_button_;
  gfx::Point mouse_location = button->GetLocalBounds().CenterPoint();

  ui::MouseEvent press_event(ui::EventType::kMousePressed, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  // Mouse release will spawn a menu which we will then close.
  ui::MouseEvent release_event(ui::EventType::kMouseReleased, mouse_location,
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

TEST_F(ShelfViewInkDropTest, DismissingMenuWithDoubleClickDoesntShowInkDrop) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  InitBrowserButtonInkDrop();

  views::Button* button = browser_button_;

  // Show a context menu on the home button.
  generator->MoveMouseTo(shelf_view_->shelf_widget()
                             ->navigation_widget()
                             ->GetHomeButton()
                             ->GetBoundsInScreen()
                             .CenterPoint());
  generator->PressRightButton();
  generator->ReleaseRightButton();
  EXPECT_TRUE(shelf_view_->IsShowingMenu());

  // Now check that double-clicking on the browser button dismisses the context
  // menu, and does not show an ink drop.
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  generator->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator->DoubleClickLeftButton();
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
}

// Tests that the shelf ink drop transforms when the host transforms. Regression
// test for https://crbug.com/1097044.
TEST_F(ShelfViewInkDropTest, ShelfButtonTransformed) {
  InitBrowserButtonInkDrop();
  ASSERT_TRUE(browser_button_ink_drop_impl_);
  auto ink_drop_impl_test_api =
      std::make_unique<views::test::InkDropImplTestApi>(
          browser_button_ink_drop_impl_);

  views::Button* button = browser_button_;
  gfx::Transform transform;
  transform.Translate(10, 0);
  button->SetTransform(transform);
  EXPECT_EQ(transform, ink_drop_impl_test_api->GetRootLayer()->transform());

  button->SetTransform(gfx::Transform());
  EXPECT_EQ(gfx::Transform(),
            ink_drop_impl_test_api->GetRootLayer()->transform());
}

class ShelfViewFocusTest : public ShelfViewTest {
 public:
  ShelfViewFocusTest() = default;

  ShelfViewFocusTest(const ShelfViewFocusTest&) = delete;
  ShelfViewFocusTest& operator=(const ShelfViewFocusTest&) = delete;

  ~ShelfViewFocusTest() override = default;

  // AshTestBase:
  void SetUp() override {
    ShelfViewTest::SetUp();

    // Add two app shortcuts for testing.
    AddAppShortcut();
    AddAppShortcut();

    // Focus the home button.
    Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
    shelf->shelf_focus_cycler()->FocusNavigation(false /* last_element */);
  }

  void DoTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  }

  void DoShiftTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  }

  void DoEnter() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  }
};

// Tests that the number of buttons is as expected and the shelf's widget
// intially has focus.
TEST_F(ShelfViewFocusTest, Basic) {
  // There are five buttons, including 3 app buttons. The back button and
  // launcher are always there, the browser shortcut is added in
  // ShelfViewTest and the two test apps added in ShelfViewFocusTest.
  EXPECT_EQ(3u, test_api_->GetButtonCount());
  EXPECT_TRUE(GetPrimaryShelf()->navigation_widget()->IsActive());

  // The home button is focused initially because the back button is only
  // visible in tablet mode.
  EXPECT_TRUE(shelf_view_->shelf_widget()
                  ->navigation_widget()
                  ->GetHomeButton()
                  ->HasFocus());
}

// Tests that the expected views have focus when cycling through shelf items
// with tab.
TEST_F(ShelfViewFocusTest, ForwardCycling) {
  // Pressing tab once should advance focus to the next element after the
  // home button, which is the first app.
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(0)->HasFocus());

  DoTab();
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(2)->HasFocus());
}

// Tests that the expected views have focus when cycling backwards through shelf
// items with shift tab.
TEST_F(ShelfViewFocusTest, BackwardCycling) {
  // The first element is currently focused. Let's advance to the last element
  // first.
  EXPECT_TRUE(shelf_view_->shelf_widget()
                  ->navigation_widget()
                  ->GetHomeButton()
                  ->HasFocus());
  DoTab();
  DoTab();
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(2)->HasFocus());

  // Pressing shift tab once should advance focus to the previous element.
  DoShiftTab();
  EXPECT_TRUE(test_api_->GetViewAt(1)->HasFocus());
}

// Verifies that focus moves as expected between the shelf and the status area.
TEST_F(ShelfViewFocusTest, FocusCyclingBetweenShelfAndStatusWidget) {
  // The first element of the shelf (the home button) is focused at start.
  EXPECT_TRUE(shelf_view_->shelf_widget()
                  ->navigation_widget()
                  ->GetHomeButton()
                  ->HasFocus());

  // Focus the next few elements.
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(0)->HasFocus());
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(1)->HasFocus());
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(2)->HasFocus());

  // This is the last element. Tabbing once more should go into the status
  // area. If calendar view is enabled it is focusing on the date tray.
  DoTab();
  ExpectNotFocused(shelf_view_);
  ExpectFocused(status_area_);

  // Shift-tab: we should be back at the last element in the shelf.
  DoShiftTab();
  EXPECT_TRUE(test_api_->GetViewAt(2)->HasFocus());
  ExpectNotFocused(status_area_);

  // Go into the status area again.
  DoTab();
  ExpectNotFocused(shelf_view_);
  ExpectFocused(status_area_);

  // Move the focusing ring from the date tray to the unified tray.
  DoTab();
  ExpectNotFocused(shelf_view_);
  ExpectFocused(status_area_);

  // And keep going forward, now we should be cycling back to the first shelf
  // element.
  DoTab();
  EXPECT_TRUE(shelf_view_->shelf_widget()
                  ->navigation_widget()
                  ->GetHomeButton()
                  ->HasFocus());
  ExpectNotFocused(status_area_);
}

// Verifies that hitting the Esc key can consistently unfocus the shelf.
TEST_F(ShelfViewFocusTest, UnfocusWithEsc) {
  // The home button is focused at start.
  EXPECT_TRUE(shelf_view_->shelf_widget()
                  ->navigation_widget()
                  ->GetHomeButton()
                  ->HasFocus());

  // Focus the status area.
  DoShiftTab();
  ExpectNotFocused(shelf_view_);
  ExpectFocused(status_area_);

  // Move the focusing ring from the unified tray to the date tray.
  DoShiftTab();
  ExpectNotFocused(shelf_view_);
  ExpectFocused(status_area_);

  // Advance backwards to the last element of the shelf.
  DoShiftTab();
  ExpectNotFocused(status_area_);
  ExpectFocused(shelf_view_);

  // Press Escape. Nothing should be focused.
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE,
                         ui::EF_NONE);
  shelf_view_->GetWidget()->OnKeyEvent(&key_event);
  ExpectNotFocused(status_area_);
  ExpectNotFocused(shelf_view_);
}

class ShelfViewFocusWithNoShelfNavigationTest : public ShelfViewFocusTest {
 public:
  ShelfViewFocusWithNoShelfNavigationTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kHideShelfControlsInTabletMode}, {});
  }
  ~ShelfViewFocusWithNoShelfNavigationTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ShelfViewFocusWithNoShelfNavigationTest,
       ShelfWithoutNavigationControls) {
  // The home button is focused at start.
  ASSERT_TRUE(GetPrimaryShelf()->navigation_widget()->GetHomeButton());
  EXPECT_TRUE(shelf_view_->shelf_widget()
                  ->navigation_widget()
                  ->GetHomeButton()
                  ->HasFocus());

  // Switch to tablet mode, which should hide navigation buttons.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  test_api_->RunMessageLoopUntilAnimationsDone();

  ASSERT_FALSE(GetPrimaryShelf()->navigation_widget()->GetHomeButton());
  ExpectFocused(status_area_);

  // Verify focus cycling skips the navigation widget.
  DoTab();
  ExpectFocused(status_area_);
  DoTab();
  ExpectFocused(shelf_view_);
  DoShiftTab();
  ExpectFocused(status_area_);
}

class ShelfViewGestureTapTest : public ShelfViewTest {
 public:
  ShelfViewGestureTapTest()
      : ShelfViewTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ShelfViewGestureTapTest() override = default;

  // ShelfViewTest:
  void SetUp() override {
    ShelfViewTest::SetUp();

    app_icon1_ = GetButtonByID(AddAppShortcut());
    app_icon2_ = GetButtonByID(AddAppShortcut());
  }

  views::InkDropState GetInkDropStateOfAppIcon1() const {
    return views::InkDrop::Get(app_icon1_)
        ->GetInkDrop()
        ->GetTargetInkDropState();
  }

 protected:
  raw_ptr<ShelfAppButton, DanglingUntriaged> app_icon1_ = nullptr;
  raw_ptr<ShelfAppButton, DanglingUntriaged> app_icon2_ = nullptr;
};

// Verifies the shelf app button's inkdrop behavior when the mouse click
// occurs after gesture long press but before the end of gesture.
TEST_F(ShelfViewGestureTapTest, MouseClickInterruptionAfterGestureLongPress) {
  const gfx::Point app_icon1_center_point =
      app_icon1_->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->PressTouch(app_icon1_center_point);

  // Fast forward to generate the EventType::kGestureShowPress event.
  task_environment()->FastForwardBy(base::Milliseconds(200));

  // Fast forward to generate the EventType::kGestureLongPress event to show the
  // context menu.
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  ASSERT_TRUE(shelf_view_->IsShowingMenu());

  // Mouse click at `app_icon2_` while gesture pressing `app_icon1_`.
  GetEventGenerator()->MoveMouseTo(
      app_icon2_->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // Release the gesture press on `app_icon1_`.
  GetEventGenerator()->set_current_screen_location(app_icon1_center_point);
  GetEventGenerator()->ReleaseTouch();

  // Verify that the context menu shows and `app_icon1_`'s inkdrop is activated.
  EXPECT_TRUE(shelf_view_->IsShowingMenu());
  EXPECT_EQ(views::InkDropState::ACTIVATED, GetInkDropStateOfAppIcon1());

  // Click at the mouse left button at an empty space. Verify that the context
  // menu is closed and the inkdrop is deactivated.
  GetEventGenerator()->MoveMouseTo(GetPrimaryDisplay().bounds().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_EQ(views::InkDropState::HIDDEN, GetInkDropStateOfAppIcon1());
}

// Verifies that tap gesture targets the correct app item view if the previous
// tap gesture resulted in a system modal window being shown.
TEST_F(ShelfViewGestureTapTest, TapAfterShowingSystemModalWindow) {
  // Add two shelf app buttons.
  const ShelfID id1 = AddAppShortcut();
  const ShelfID id2 = AddAppShortcut();

  auto item_1_delegate_owned =
      std::make_unique<SystemModalWindowShelfItemDelegate>(base::BindRepeating(
          &AshTestBase::CreateTestWindow, base::Unretained(this)));
  SystemModalWindowShelfItemDelegate* item_1_delegate =
      item_1_delegate_owned.get();
  model_->ReplaceShelfItemDelegate(id1, std::move(item_1_delegate_owned));

  auto item_2_delegate_owned = std::make_unique<ShelfItemSelectionTracker>();
  ShelfItemSelectionTracker* item_2_delegate = item_2_delegate_owned.get();
  model_->ReplaceShelfItemDelegate(id2, std::move(item_2_delegate_owned));

  GetEventGenerator()->GestureTapAt(
      GetButtonByID(id1)->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(item_1_delegate->HasTestWindow());
  ASSERT_TRUE(Shell::IsSystemModalWindowOpen());
  item_1_delegate->ResetTestWindow();

  GetEventGenerator()->GestureTapAt(
      GetButtonByID(id2)->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, item_2_delegate->item_selected_count());
  EXPECT_FALSE(item_1_delegate->HasTestWindow());
}

// Verifies that removing an item that is still waiting for the context menu
// model works as expected.
TEST_F(ShelfViewGestureTapTest, InterruptContextMenuShowByItemRemoval) {
  // Add two shelf app buttons.
  const ShelfID id1 = AddAppShortcut();
  const ShelfID id2 = AddAppShortcut();

  auto item_delegate_owned =
      std::make_unique<AsyncContextMenuShelfItemDelegate>();
  AsyncContextMenuShelfItemDelegate* item_delegate = item_delegate_owned.get();
  model_->ReplaceShelfItemDelegate(id1, std::move(item_delegate_owned));

  ShelfAppButton* app_button = GetButtonByID(id1);
  GetEventGenerator()->MoveTouch(app_button->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressTouch();

  // Fast forward to generate the EventType::kGestureShowPress event.
  task_environment()->FastForwardBy(base::Milliseconds(200));

  // Fast forward to generate the EventType::kGestureLongPress event to show the
  // context menu.
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());

  // Remove the shelf item indexed by `id` before handling the pending context
  // menu model request.
  const int index = ShelfModel::Get()->ItemIndexByID(id1);
  ShelfModel::Get()->RemoveItemAt(index);
  EXPECT_FALSE(shelf_view_->drag_view());

  // Initialize the mouse drag on the shelf app button specified by `id2`.
  ShelfAppButton* app_button2 = GetButtonByID(id2);
  // Wait for app 2 to move in reaction to removing app 1.
  test_api_->RunMessageLoopUntilAnimationsDone();
  GetEventGenerator()->MoveMouseTo(
      app_button2->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressLeftButton();
  task_environment()->FastForwardBy(base::Milliseconds(200));

  // Move the mouse. Verify that the shelf view has a view under drag.
  GetEventGenerator()->MoveMouseBy(0, -100);
  EXPECT_TRUE(shelf_view_->drag_view());
}

TEST_F(ShelfViewGestureTapTest,
       PressEscapeKeyBeforeReleaseLongPressOnAppButton) {
  const ShelfID id = AddAppShortcut();
  auto item_delegate_owned =
      std::make_unique<AsyncContextMenuShelfItemDelegate>();
  AsyncContextMenuShelfItemDelegate* item_delegate = item_delegate_owned.get();
  model_->ReplaceShelfItemDelegate(id, std::move(item_delegate_owned));

  ShelfAppButton* app_button = GetButtonByID(id);
  GetEventGenerator()->MoveTouch(app_button->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressTouch();

  // Fast forward to generate the EventType::kGestureShowPress event.
  task_environment()->FastForwardBy(base::Milliseconds(200));

  // Fast forward to generate the EventType::kGestureLongPress event to show the
  // context menu.
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());

  // Build a dummy context menu and show it.
  {
    auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
    menu_model->AddItem(203, u"item");
    item_delegate->RunPendingContextMenuCallback(std::move(menu_model));
    EXPECT_TRUE(shelf_view_->IsShowingMenuForView(app_button));
  }

  // Press Escape. The context menu should be closed.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(item_delegate->HasPendingContextMenuCallback());

  // Release the gesture press. The context menu should show again.
  GetEventGenerator()->ReleaseTouch();
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());
  {
    auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
    menu_model->AddItem(203, u"item");
    item_delegate->RunPendingContextMenuCallback(std::move(menu_model));
    EXPECT_TRUE(shelf_view_->IsShowingMenuForView(app_button));
  }

  // Verify that the ink drop of the app button for which the context menu shows
  // for is activated.
  EXPECT_EQ(
      views::InkDropState::ACTIVATED,
      views::InkDrop::Get(app_button)->GetInkDrop()->GetTargetInkDropState());
}

// Verifies the shelf app button's inkdrop behavior when the mouse click
// occurs before gesture long press.
TEST_F(ShelfViewGestureTapTest, MouseClickInterruptionBeforeGestureLongPress) {
  const gfx::Point app_icon1_center_point =
      app_icon1_->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->PressTouch(app_icon1_center_point);

  // Fast forward to generate the EventType::kGestureShowPress event.
  task_environment()->FastForwardBy(base::Milliseconds(200));

  // Mouse click at `app_icon2_` while gesture pressing `app_icon1_`. Note that
  // we do not need to release the touch on `app_icon1_` because the gesture
  // is interrupted by the mouse click.
  GetEventGenerator()->MoveMouseTo(
      app_icon2_->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // Fast forward until the callback is executed.
  task_environment()->FastForwardBy(base::Milliseconds(200));

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_EQ(views::InkDropState::HIDDEN, GetInkDropStateOfAppIcon1());
}

// Test class to test the desk button.
class ShelfViewDeskButtonTest : public ShelfViewTest {
 public:
  ShelfViewDeskButtonTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kDeskButton);
  }

  ShelfViewDeskButtonTest(const ShelfViewDeskButtonTest&) = delete;
  ShelfViewDeskButtonTest& operator=(const ShelfViewDeskButtonTest&) = delete;

  ~ShelfViewDeskButtonTest() override = default;

  DeskButtonWidget* desk_button_widget() {
    return test_api_->shelf_view()->shelf_widget()->desk_button_widget();
  }

  // ShelfViewTest:
  void SetUp() override {
    ShelfViewTest::SetUp();

    // With the desk button, there should be space in the hotseat for 6 apps.
    ASSERT_GE(GetPrimaryShelf()
                  ->shelf_widget()
                  ->hotseat_widget()
                  ->GetWindowBoundsInScreen()
                  .width(),
              336);
    prefs_ = Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  }

  raw_ptr<PrefService, DanglingUntriaged> prefs_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verify that the desk button is visible outside of overview, and not visible
// in overview mode.
TEST_F(ShelfViewDeskButtonTest, OverviewVisibility) {
  SetShowDeskButtonInShelfPref(prefs_, true);
  // The button should be visible.
  EXPECT_TRUE(desk_button_widget()->GetLayer()->GetTargetVisibility());
  const int original_hotseat_width = GetPrimaryShelf()
                                         ->shelf_widget()
                                         ->hotseat_widget()
                                         ->GetWindowBoundsInScreen()
                                         .width();

  // The button should disappear in overview mode and reappear after.
  ToggleOverview();
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());
  // To avoid unnecessary re-layout before/after overview, keep hotseat the same
  // width.
  EXPECT_EQ(GetPrimaryShelf()
                ->shelf_widget()
                ->hotseat_widget()
                ->GetWindowBoundsInScreen()
                .width(),
            original_hotseat_width);
  ToggleOverview();
  EXPECT_TRUE(desk_button_widget()->GetLayer()->GetTargetVisibility());

  // Repeat for vertical alignment.
  test_api_->shelf_view()->shelf()->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_TRUE(desk_button_widget()->GetLayer()->GetTargetVisibility());
  ToggleOverview();
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());
  ToggleOverview();
  EXPECT_TRUE(desk_button_widget()->GetLayer()->GetTargetVisibility());
}

// Verify that the desk button is not visible in tablet mode.
TEST_F(ShelfViewDeskButtonTest, TabletModeVisibility) {
  SetShowDeskButtonInShelfPref(prefs_, true);
  EXPECT_TRUE(desk_button_widget()->GetLayer()->GetTargetVisibility());

  // In tablet mode, the shelf should be visible but the desk button shouldn't.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_EQ(SHELF_VISIBLE, GetPrimaryShelf()->GetVisibilityState());
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());

  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_TRUE(desk_button_widget()->GetLayer()->GetTargetVisibility());
}

// Verify that the desk button does not appear by default, appears when the user
// has more than 1 desk, and stays even if they go back to having just one desk.
TEST_F(ShelfViewDeskButtonTest, NewDeskVisibility) {
  // By default the visibility pref should be `kNotSet`, the device uses desks
  // pref should be false, and the button should not be visible.
  EXPECT_EQ(prefs_->GetString(prefs::kShowDeskButtonInShelf), "");
  EXPECT_FALSE(prefs_->GetBoolean(prefs::kDeviceUsesDesks));
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());

  // Going into and out of tablet mode and overview shouldn't change this.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(prefs_->GetString(prefs::kShowDeskButtonInShelf), "");
  EXPECT_FALSE(prefs_->GetBoolean(prefs::kDeviceUsesDesks));
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());

  ToggleOverview();
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());
  ToggleOverview();
  EXPECT_EQ(prefs_->GetString(prefs::kShowDeskButtonInShelf), "");
  EXPECT_FALSE(prefs_->GetBoolean(prefs::kDeviceUsesDesks));
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());

  // Adding one desk should change the device uses desks pref to true, and it
  // should stay that way when the desk is removed. The desk button visibility
  // pref should not be changed.
  Shell::Get()->desks_controller()->NewDesk(
      DesksCreationRemovalSource::kButton);
  EXPECT_EQ(prefs_->GetString(prefs::kShowDeskButtonInShelf), "");
  EXPECT_TRUE(prefs_->GetBoolean(prefs::kDeviceUsesDesks));
  EXPECT_TRUE(desk_button_widget()->GetLayer()->GetTargetVisibility());

  RemoveDesk(Shell::Get()->desks_controller()->GetTargetActiveDesk(),
             DeskCloseType::kCloseAllWindows);
  EXPECT_EQ(prefs_->GetString(prefs::kShowDeskButtonInShelf), "");
  EXPECT_TRUE(prefs_->GetBoolean(prefs::kDeviceUsesDesks));
  EXPECT_TRUE(desk_button_widget()->GetLayer()->GetTargetVisibility());
}

// Verify that if the user hides the desk button, the button will never show
// unless the user elects to show the button manually.
TEST_F(ShelfViewDeskButtonTest, PrefHidden) {
  SetShowDeskButtonInShelfPref(prefs_, false);
  SetDeviceUsesDesksPref(prefs_, false);
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());

  // Adding a desk should not make the button visible.
  Shell::Get()->desks_controller()->NewDesk(
      DesksCreationRemovalSource::kButton);
  EXPECT_EQ(prefs_->GetString(prefs::kShowDeskButtonInShelf), "Hidden");
  EXPECT_TRUE(prefs_->GetBoolean(prefs::kDeviceUsesDesks));
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());

  // Removing the desk, cycling overview, and tablet mode should not make it
  // visible.

  RemoveDesk(Shell::Get()->desks_controller()->GetTargetActiveDesk(),
             DeskCloseType::kCloseAllWindows);
  EXPECT_EQ(prefs_->GetString(prefs::kShowDeskButtonInShelf), "Hidden");
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());

  ToggleOverview();
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());
  ToggleOverview();
  EXPECT_EQ(prefs_->GetString(prefs::kShowDeskButtonInShelf), "Hidden");
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());

  ash::TabletModeControllerTestApi().EnterTabletMode();
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(prefs_->GetString(prefs::kShowDeskButtonInShelf), "Hidden");
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());

  // Setting the pref to shown after being hidden should make the button
  // visible.
  SetShowDeskButtonInShelfPref(prefs_, true);
  EXPECT_EQ(prefs_->GetString(prefs::kShowDeskButtonInShelf), "Shown");
  EXPECT_TRUE(desk_button_widget()->GetLayer()->GetTargetVisibility());

  // Setting the pref to hidden after being shown should make the button
  // disappear.
  SetShowDeskButtonInShelfPref(prefs_, false);
  EXPECT_EQ(prefs_->GetString(prefs::kShowDeskButtonInShelf), "Hidden");
  EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());
}

// Verify that the correct combination of values for the two desk button
// visibility preferences result in the desk button being shown and not shown.
TEST_F(ShelfViewDeskButtonTest, PrefVisibilityRelationship) {
  for (std::string visibility : {"", "Shown", "Hidden"}) {
    for (bool uses_desks : {true, false}) {
      prefs_->SetString(prefs::kShowDeskButtonInShelf, visibility);
      SetDeviceUsesDesksPref(prefs_, uses_desks);
      if (uses_desks) {
        if (visibility == "Hidden") {
          EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());
        } else {
          EXPECT_TRUE(desk_button_widget()->GetLayer()->GetTargetVisibility());
        }
      } else {
        if (visibility == "Shown") {
          EXPECT_TRUE(desk_button_widget()->GetLayer()->GetTargetVisibility());
        } else {
          EXPECT_FALSE(desk_button_widget()->GetLayer()->GetTargetVisibility());
        }
      }
    }
  }
}

// Verify that metrics are being correctly recorded for when a user hides the
// desk button.
TEST_F(ShelfViewDeskButtonTest, VisibilityMetrics) {
  SetShowDeskButtonInShelfPref(prefs_, true);
  base::HistogramTester histogram_tester;
  ShelfContextMenuModel menu_model(nullptr, GetPrimaryDisplayId(),
                                   /*menu_in_shelf=*/false);
  menu_model.ExecuteCommand(
      static_cast<int>(ShelfContextMenuModel::CommandId::MENU_HIDE_DESK_NAME),
      /*event_flags=*/0);
  histogram_tester.ExpectTotalCount(kDeskButtonHiddenHistogramName, 1);
}

class ShelfViewPromiseAppTest : public ShelfViewTest {
 public:
  ShelfViewPromiseAppTest() = default;
  ShelfViewPromiseAppTest(const ShelfViewPromiseAppTest&) = delete;
  ShelfViewPromiseAppTest& operator=(const ShelfViewPromiseAppTest&) = delete;
  ~ShelfViewPromiseAppTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kPromiseIcons};
};

TEST_F(ShelfViewPromiseAppTest, UpdateProgressOnPromiseIcon) {
  // Add platform app button.
  ShelfID last_added = AddAppShortcut();
  ShelfItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  ShelfAppButton* button = GetButtonByID(last_added);

  item.app_status = AppStatus::kPending;
  item.progress = 0.0f;
  item.is_promise_app = true;
  model_->Set(index, item);

  // Start install progress bar.
  item.app_status = AppStatus::kInstalling;
  item.progress = 0.0f;
  model_->Set(index, item);
  ProgressIndicator* progress_indicator = button->GetProgressIndicatorForTest();
  ASSERT_TRUE(progress_indicator);

  EXPECT_EQ(item.progress, 0.f);
  EXPECT_EQ(button->progress(), 0.f);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.0f);

  item.app_status = AppStatus::kInstalling;
  item.progress = 0.3f;
  model_->Set(index, item);
  EXPECT_EQ(item.progress, 0.3f);
  EXPECT_EQ(button->progress(), 0.3f);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.3f);

  item.app_status = AppStatus::kInstalling;
  item.progress = 0.7f;
  model_->Set(index, item);
  EXPECT_EQ(item.progress, 0.7f);
  EXPECT_EQ(button->progress(), 0.7f);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.7f);

  item.app_status = AppStatus::kInstalling;
  item.progress = 1.5f;
  model_->Set(index, item);
  EXPECT_EQ(item.progress, 1.5f);
  EXPECT_EQ(button->progress(), 1.5f);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 1.0f);
}

TEST_F(ShelfViewPromiseAppTest, AccessibleDescription) {
  // Add platform app button.
  ShelfID last_added = AddAppShortcut();
  ShelfItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  ShelfAppButton* button = GetButtonByID(last_added);

  EXPECT_EQ(u"", button->GetViewAccessibility().GetCachedDescription());

  item.app_status = AppStatus::kBlocked;
  item.progress = 0.0f;
  item.is_promise_app = true;
  model_->Set(index, item);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SHELF_ITEM_BLOCKED_APP),
            button->GetViewAccessibility().GetCachedDescription());

  item.app_status = AppStatus::kPaused;
  item.progress = 0.0f;
  model_->Set(index, item);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SHELF_ITEM_PAUSED_APP),
            button->GetViewAccessibility().GetCachedDescription());

  item.app_status = AppStatus::kInstalling;
  item.progress = 0.3f;
  model_->Set(index, item);
  EXPECT_EQ(u"", button->GetViewAccessibility().GetCachedDescription());
}

TEST_F(ShelfViewPromiseAppTest, AccessibleName) {
  ShelfID last_added = AddAppShortcut();
  ShelfItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  ShelfAppButton* button = GetButtonByID(last_added);

  // Default title of the shelf item should be updated as the accessible name of
  // the shelf app button.
  ui::AXNodeData data;
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            item.title);

  // On updating the item's title the accessible name of the shelf app button
  // should also be updated.
  item.title = u"Test app";
  model_->Set(index, item);

  data = ui::AXNodeData();
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            item.title);

  // If a non-empty accessible name of the shelf item itself is being provided
  // in that case the preference should be given to item's accessible name
  item.accessible_name = u"Test accessible name";
  model_->Set(index, item);

  data = ui::AXNodeData();
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            item.accessible_name);

  // Passing an empty accessible to the item should once again give the
  // preference back to item's title.
  item.accessible_name = u"";
  model_->Set(index, item);

  data = ui::AXNodeData();
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            item.title);
}

TEST_F(ShelfViewPromiseAppTest, AppStatusReflectsOnProgressIndicator) {
  // Add platform app button.
  ShelfID last_added = AddAppShortcut();
  ShelfItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  ShelfAppButton* button = GetButtonByID(last_added);

  // Promise apps are created with app_status kPending.
  item.app_status = AppStatus::kPending;
  item.is_promise_app = true;
  model_->Set(index, item);

  ProgressIndicator* progress_indicator = button->GetProgressIndicatorForTest();
  ASSERT_TRUE(progress_indicator);
  // Change app status to installing and send a progress update. Verify that the
  // progress indicator correctly reflects the progress.
  EXPECT_EQ(button->progress(), -1.0f);
  EXPECT_EQ(button->app_status(), AppStatus::kPending);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.0f);

  // Start install progress bar.
  item.app_status = AppStatus::kInstalling;
  item.progress = 0.3f;
  model_->Set(index, item);

  EXPECT_EQ(button->progress(), 0.3f);
  EXPECT_EQ(button->app_status(), AppStatus::kInstalling);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.3f);

  // Change app status back to pending state. Verify that even if the item had
  // progress previously associated to it, the progress indicator reflects as
  // 0 progress since it is pending.
  item.app_status = AppStatus::kPending;
  model_->Set(index, item);
  EXPECT_EQ(item.progress, 0.3f);
  EXPECT_EQ(button->progress(), 0.3f);
  EXPECT_EQ(button->app_status(), AppStatus::kPending);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.0f);

  // Send another progress update. Since the app status is still pending, the
  // progress indicator still be 0.
  item.progress = 0.7f;
  model_->Set(index, item);
  EXPECT_EQ(item.progress, 0.7f);
  EXPECT_EQ(button->progress(), 0.7f);
  EXPECT_EQ(button->app_status(), AppStatus::kPending);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.0f);

  // Set the last status update to kReady as if the app had finished installing.
  item.app_status = AppStatus::kReady;
  model_->Set(index, item);
  EXPECT_EQ(button->app_status(), AppStatus::kReady);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.0f);
}

TEST_F(ShelfViewPromiseAppTest, PromiseIconLayers) {
  // Add platform app button.
  ShelfID last_added = AddAppShortcut();
  const std::string promise_app_id = last_added.app_id;
  ShelfItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  ShelfAppButton* button = GetButtonByID(last_added);

  // Promise apps are created with app_status kPending.
  item.app_status = AppStatus::kPending;
  item.is_promise_app = true;
  model_->Set(index, item);

  ProgressIndicator* progress_indicator = button->GetProgressIndicatorForTest();
  ASSERT_TRUE(progress_indicator);
  // Change app status to installing and send a progress update. Verify that the
  // progress indicator correctly reflects the progress.
  EXPECT_EQ(button->progress(), -1.0f);
  EXPECT_EQ(button->app_status(), AppStatus::kPending);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.0f);

  // Start install progress bar.
  item.app_status = AppStatus::kInstalling;
  item.progress = 0.3f;
  model_->Set(index, item);

  EXPECT_EQ(button->progress(), 0.3f);
  EXPECT_EQ(button->app_status(), AppStatus::kInstalling);
  EXPECT_TRUE(button->layer());

  // Set the last status update to kInstallSuccess as if the app had finished
  // installing.
  item.app_status = AppStatus::kInstallSuccess;
  model_->Set(index, item);
  EXPECT_EQ(button->app_status(), AppStatus::kInstallSuccess);
  EXPECT_TRUE(button->layer());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Simulate pushing the installed app.
  model_->RemoveItemAt(index);

  ASSERT_TRUE(test_api_->HasPendingPromiseAppRemoval(promise_app_id));

  {
    ShelfItem installed_item;
    installed_item.id = ShelfID("foo");
    installed_item.title = u"Test app";
    installed_item.type = TYPE_APP;
    installed_item.package_id = promise_app_id;
    ShelfModel::Get()->Add(
        installed_item,
        std::make_unique<TestShelfItemDelegate>(installed_item.id));
    ShelfAppButton* installed_button = GetButtonByID(installed_item.id);

    ASSERT_TRUE(installed_button->layer());
    EXPECT_TRUE(test_api_->HasPendingPromiseAppRemoval(promise_app_id));

    // Verify that the icon layer is animating.
    EXPECT_FALSE(installed_button->layer()->GetAnimator()->is_animating());
    EXPECT_TRUE(
        installed_button->icon_view()->layer()->GetAnimator()->is_animating());
    EXPECT_EQ(gfx::Transform(), installed_button->icon_view()
                                    ->layer()
                                    ->GetAnimator()
                                    ->GetTargetTransform());
    LayerAnimationWaiter animation_waiter(
        installed_button->icon_view()->layer()->GetAnimator());
    animation_waiter.Wait();
  }

  EXPECT_FALSE(test_api_->HasPendingPromiseAppRemoval(promise_app_id));
}

}  // namespace ash
