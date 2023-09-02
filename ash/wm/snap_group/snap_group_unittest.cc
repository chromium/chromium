// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_expanded_menu_view.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_divider_view.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_cycle/window_cycle_list.h"
#include "ash/wm/window_cycle/window_cycle_view.h"
#include "ash/wm/window_mini_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/multi_window_resize_controller.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

using ::ui::mojom::CursorType;

using WindowCyclingDirection = WindowCycleController::WindowCyclingDirection;

constexpr int kWindowMiniViewCornerRadius = 16;

SplitViewController* split_view_controller() {
  return SplitViewController::Get(Shell::GetPrimaryRootWindow());
}

SplitViewDivider* split_view_divider() {
  return split_view_controller()->split_view_divider();
}

gfx::Rect split_view_divider_bounds_in_screen() {
  return split_view_divider()->GetDividerBoundsInScreen(
      /*is_dragging=*/false);
}

const gfx::Rect work_area_bounds() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
}

IconButton* kebab_button() {
  SplitViewDividerView* divider_view =
      split_view_divider()->divider_view_for_testing();
  CHECK(divider_view);
  return divider_view->kebab_button_for_testing();
}

views::Widget* snap_group_expanded_menu_widget() {
  SplitViewDividerView* divider_view =
      split_view_divider()->divider_view_for_testing();
  CHECK(divider_view);
  return divider_view->snap_group_expanded_menu_widget_for_testing();
}

SnapGroupExpandedMenuView* snap_group_expanded_menu_view() {
  SplitViewDividerView* divider_view =
      split_view_divider()->divider_view_for_testing();
  CHECK(divider_view);
  return divider_view->snap_group_expanded_menu_view_for_testing();
}

IconButton* swap_windows_button() {
  DCHECK(snap_group_expanded_menu_view());
  return snap_group_expanded_menu_view()->swap_windows_button_for_testing();
}

IconButton* update_primary_window_button() {
  DCHECK(snap_group_expanded_menu_view());
  return snap_group_expanded_menu_view()
      ->update_primary_window_button_for_testing();
}

IconButton* update_secondary_window_button() {
  DCHECK(snap_group_expanded_menu_view());
  return snap_group_expanded_menu_view()
      ->update_secondary_window_button_for_testing();
}

IconButton* unlock_button() {
  DCHECK(snap_group_expanded_menu_view());
  return snap_group_expanded_menu_view()->unlock_button_for_testing();
}

void SwitchToTabletMode() {
  TabletModeControllerTestApi test_api;
  test_api.DetachAllMice();
  test_api.EnterTabletMode();
}

void ExitTabletMode() {
  TabletModeControllerTestApi().LeaveTabletMode();
}

void WaitForSeconds(int seconds) {
  base::RunLoop loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Seconds(seconds));
  loop.Run();
}

gfx::Rect GetOverviewGridBounds() {
  OverviewSession* overview_session = GetOverviewSession();
  if (!overview_session) {
    return gfx::Rect();
  }

  return overview_session->grid_list()[0]->bounds_for_testing();
}

}  // namespace

class SnapGroupTest : public AshTestBase {
 public:
  SnapGroupTest() {
    // Temporarily disable the `AutomaticLockGroup` feature params(enable by
    // default), as the param is true by default.
    // TODO(michelefan@): Change it back to
    // `scoped_feature_list_.InitAndEnableFeature(features::kSnapGroup)` when
    // do the refactor work for the snap group unit tests.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kSnapGroup, {{"AutomaticLockGroup", "false"}}},
         {chromeos::features::kJellyroll, {}},
         {chromeos::features::kJelly, {}},
         {features::kSameAppWindowCycle, {}}},
        {});
  }
  SnapGroupTest(const SnapGroupTest&) = delete;
  SnapGroupTest& operator=(const SnapGroupTest&) = delete;
  ~SnapGroupTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    WorkspaceEventHandler* event_handler =
        WorkspaceControllerTestApi(ShellTestApi().workspace_controller())
            .GetEventHandler();
    resize_controller_ = event_handler->multi_window_resize_controller();
    WindowCycleList::SetDisableInitialDelayForTesting(true);
  }

  void SnapOneTestWindow(aura::Window* window,
                         chromeos::WindowStateType state_type) {
    UpdateDisplay("800x600");
    WindowState* window_state = WindowState::Get(window);
    const WindowSnapWMEvent snap_type(
        state_type == chromeos::WindowStateType::kPrimarySnapped
            ? WM_EVENT_SNAP_PRIMARY
            : WM_EVENT_SNAP_SECONDARY);
    window_state->OnWMEvent(&snap_type);
    EXPECT_EQ(state_type, window_state->GetStateType());
  }

  // Verifies that the icon image and the tooltip of the lock button reflect the
  // `locked` or `unlocked` state.
  void VerifyLockButton(bool locked, IconButton* lock_button) {
    const SkColor color =
        lock_button->GetColorProvider()->GetColor(kColorAshIconColorPrimary);
    const gfx::ImageSkia locked_icon_image =
        gfx::CreateVectorIcon(kLockScreenEasyUnlockCloseIcon, color);
    const gfx::ImageSkia unlocked_icon_image =
        gfx::CreateVectorIcon(kLockScreenEasyUnlockOpenIcon, color);
    const SkBitmap* expected_icon =
        locked ? unlocked_icon_image.bitmap() : locked_icon_image.bitmap();
    const SkBitmap* actual_icon =
        lock_button->GetImage(views::ImageButton::ButtonState::STATE_NORMAL)
            .bitmap();
    EXPECT_TRUE(gfx::test::AreBitmapsEqual(*actual_icon, *expected_icon));

    const auto expected_tooltip_string = l10n_util::GetStringUTF16(
        locked ? IDS_ASH_SNAP_GROUP_CLICK_TO_UNLOCK_WINDOWS
               : IDS_ASH_SNAP_GROUP_CLICK_TO_LOCK_WINDOWS);
    EXPECT_EQ(lock_button->GetTooltipText(), expected_tooltip_string);
  }

  // Verifies that the given two windows can be locked properly and the tooltip
  // is updated accordingly.
  void PressLockWidgetToLockTwoWindows(aura::Window* window1,
                                       aura::Window* window2) {
    auto* snap_group_controller = SnapGroupController::Get();
    ASSERT_TRUE(snap_group_controller);
    EXPECT_TRUE(snap_group_controller->snap_groups_for_testing().empty());
    EXPECT_TRUE(
        snap_group_controller->window_to_snap_group_map_for_testing().empty());
    EXPECT_FALSE(
        snap_group_controller->AreWindowsInSnapGroup(window1, window2));

    auto* event_generator = GetEventGenerator();
    auto hover_location = window1->bounds().right_center();
    event_generator->MoveMouseTo(hover_location);
    auto* timer = GetShowTimer();
    EXPECT_TRUE(timer->IsRunning());
    EXPECT_TRUE(IsShowing());
    timer->FireNow();
    EXPECT_TRUE(GetLockWidget());
    VerifyLockButton(/*locked=*/false,
                     resize_controller()->lock_button_for_testing());

    gfx::Rect lock_widget_bounds(GetLockWidget()->GetWindowBoundsInScreen());
    hover_location = lock_widget_bounds.CenterPoint();
    event_generator->MoveMouseTo(hover_location);
    EXPECT_TRUE(GetLockWidget());
    event_generator->ClickLeftButton();
    EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(window1, window2));
    EXPECT_TRUE(split_view_controller()->split_view_divider());
  }

  views::Widget* GetLockWidget() const {
    DCHECK(resize_controller_);
    return resize_controller_->lock_widget_.get();
  }

  views::Widget* GetResizeWidget() const {
    DCHECK(resize_controller_);
    return resize_controller_->resize_widget_.get();
  }

  base::OneShotTimer* GetShowTimer() const {
    DCHECK(resize_controller_);
    return &resize_controller_->show_timer_;
  }

  bool IsShowing() const {
    DCHECK(resize_controller_);
    return resize_controller_->IsShowing();
  }

  MultiWindowResizeController* resize_controller() const {
    return resize_controller_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<MultiWindowResizeController, DanglingUntriaged | ExperimentalAsh>
      resize_controller_;
};

// Tests that the corresponding snap group will be created when calling
// `AddSnapGroup` and removed when calling `RemoveSnapGroup`.
TEST_F(SnapGroupTest, AddAndRemoveSnapGroupTest) {
  auto* snap_group_controller = SnapGroupController::Get();
  const auto& snap_groups = snap_group_controller->snap_groups_for_testing();
  const auto& window_to_snap_group_map =
      snap_group_controller->window_to_snap_group_map_for_testing();
  EXPECT_EQ(snap_groups.size(), 0u);
  EXPECT_EQ(window_to_snap_group_map.size(), 0u);

  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  std::unique_ptr<aura::Window> w3(CreateTestWindow());

  SnapOneTestWindow(w1.get(), chromeos::WindowStateType::kPrimarySnapped);
  SnapOneTestWindow(w2.get(), chromeos::WindowStateType::kSecondarySnapped);
  ASSERT_TRUE(snap_group_controller->AddSnapGroup(w1.get(), w2.get()));
  ASSERT_FALSE(snap_group_controller->AddSnapGroup(w1.get(), w3.get()));

  EXPECT_EQ(snap_groups.size(), 1u);
  EXPECT_EQ(window_to_snap_group_map.size(), 2u);
  const auto iter1 = window_to_snap_group_map.find(w1.get());
  ASSERT_TRUE(iter1 != window_to_snap_group_map.end());
  const auto iter2 = window_to_snap_group_map.find(w2.get());
  ASSERT_TRUE(iter2 != window_to_snap_group_map.end());
  auto* snap_group = snap_groups.back().get();
  EXPECT_EQ(iter1->second, snap_group);
  EXPECT_EQ(iter2->second, snap_group);

  ASSERT_TRUE(snap_group_controller->RemoveSnapGroup(snap_group));
  ASSERT_TRUE(snap_groups.empty());
  ASSERT_TRUE(window_to_snap_group_map.empty());
}

// Tests that the corresponding snap group will be removed when one of the
// windows in the snap group gets destroyed.
TEST_F(SnapGroupTest, WindowDestroyTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapOneTestWindow(w1.get(), chromeos::WindowStateType::kPrimarySnapped);
  SnapOneTestWindow(w2.get(), chromeos::WindowStateType::kSecondarySnapped);
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AddSnapGroup(w1.get(), w2.get()));
  const auto& snap_groups = snap_group_controller->snap_groups_for_testing();
  const auto& window_to_snap_group_map =
      snap_group_controller->window_to_snap_group_map_for_testing();
  EXPECT_EQ(snap_groups.size(), 1u);
  EXPECT_EQ(window_to_snap_group_map.size(), 2u);

  // Destroy one window in the snap group and the entire snap group will be
  // removed.
  w1.reset();
  EXPECT_TRUE(snap_groups.empty());
  EXPECT_TRUE(window_to_snap_group_map.empty());
}

// Tests that if one window in the snap group is actiaved, the stacking order of
// the other window in the snap group will be updated to be right below the
// activated window i.e. the two windows in the snap group will be placed on
// top.
TEST_F(SnapGroupTest, WindowActivationTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  std::unique_ptr<aura::Window> w3(CreateTestWindow());

  SnapOneTestWindow(w1.get(), chromeos::WindowStateType::kPrimarySnapped);
  SnapOneTestWindow(w2.get(), chromeos::WindowStateType::kSecondarySnapped);
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AddSnapGroup(w1.get(), w2.get()));

  wm::ActivateWindow(w3.get());

  // Actiave one of the windows in the snap group.
  wm::ActivateWindow(w1.get());

  MruWindowTracker::WindowList window_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  EXPECT_EQ(std::vector<aura::Window*>({w1.get(), w3.get(), w2.get()}),
            window_list);

  // `w3` is stacked below `w2` even though the activation order of `w3` is
  // before `w2`.
  // TODO(michelefan): Keep an eye out for changes in the activation logic and
  // update this test if needed in future.
  EXPECT_TRUE(window_util::IsStackedBelow(w3.get(), w2.get()));
}

// A test fixture that tests the snap group entry point arm 1 which will create
// a snap group automatically when two windows are snapped. This entry point is
// guarded by the feature flag `kSnapGroup` and will only be enabled when the
// feature param `kAutomaticallyLockGroup` is true.
class SnapGroupEntryPointArm1Test : public SnapGroupTest {
 public:
  SnapGroupEntryPointArm1Test() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kSnapGroup, {{"AutomaticLockGroup", "true"}});
  }
  SnapGroupEntryPointArm1Test(const SnapGroupEntryPointArm1Test&) = delete;
  SnapGroupEntryPointArm1Test& operator=(const SnapGroupEntryPointArm1Test&) =
      delete;
  ~SnapGroupEntryPointArm1Test() override = default;

  void SnapTwoTestWindowsInArm1(aura::Window* window1,
                                aura::Window* window2,
                                bool horizontal = true) {
    CHECK_NE(window1, window2);
    if (horizontal) {
      UpdateDisplay("800x600");
    } else {
      UpdateDisplay("600x800");
    }

    // Snap `window1` to trigger the overview session shown on the other side of
    // the screen.
    SnapOneTestWindow(
        window1,
        /*state_type=*/chromeos::WindowStateType::kPrimarySnapped);
    EXPECT_TRUE(split_view_controller()->InClamshellSplitViewMode());
    EXPECT_EQ(split_view_controller()->state(),
              SplitViewController::State::kPrimarySnapped);
    EXPECT_EQ(split_view_controller()->primary_window(), window1);
    WaitForOverviewEnterAnimation();
    EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

    // When the first window is snapped, it takes exactly half the width.
    gfx::Rect expected_bounds(work_area_bounds());
    gfx::Rect left_bounds, right_bounds;
    expected_bounds.SplitVertically(&left_bounds, &right_bounds);
    EXPECT_EQ(left_bounds, window1->GetBoundsInScreen());

    // The `window2` gets selected in the overview will be snapped to the
    // non-occupied snap position and the overview session will end.
    auto* item2 = GetOverviewItemForWindow(window2);
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        gfx::ToRoundedPoint(item2->GetTransformedBounds().CenterPoint()));
    event_generator->ClickLeftButton();
    WaitForOverviewExitAnimation();
    EXPECT_EQ(split_view_controller()->secondary_window(), window2);

    auto* snap_group_controller = SnapGroupController::Get();
    ASSERT_TRUE(snap_group_controller);
    EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(window1, window2));
    EXPECT_EQ(split_view_controller()->state(),
              SplitViewController::State::kBothSnapped);

    // The split view divider and kebab button will show on two windows snapped.
    EXPECT_TRUE(split_view_divider());
    EXPECT_TRUE(kebab_button());
    EXPECT_EQ(0.5f, *WindowState::Get(window1)->snap_ratio());
    EXPECT_EQ(0.5f, *WindowState::Get(window2)->snap_ratio());

    // Now that two windows are snapped, the divider is between them.
    gfx::Rect divider_bounds(
        split_view_divider()->GetDividerBoundsInScreen(/*is_dragging=*/false));
    left_bounds.set_width(left_bounds.width() - divider_bounds.width() / 2);
    right_bounds.set_x(right_bounds.x() + divider_bounds.width() / 2);
    right_bounds.set_width(right_bounds.width() - divider_bounds.width() / 2);
    EXPECT_EQ(left_bounds, window1->GetBoundsInScreen());
    EXPECT_EQ(right_bounds, window2->GetBoundsInScreen());
  }

  // Returns true if the union bounds of the `w1`, `w2` and split view
  // divider(if exists) equal to the bounds of the work area and false
  // otherwise.
  bool UnionBoundsEqualToWorkAreaBounds(aura::Window* w1,
                                        aura::Window* w2) const {
    gfx::Rect(union_bounds);
    union_bounds.Union(w1->GetBoundsInScreen());
    union_bounds.Union(w2->GetBoundsInScreen());
    const auto divider_bounds = split_view_divider()
                                    ? split_view_divider_bounds_in_screen()
                                    : gfx::Rect();
    union_bounds.Union(divider_bounds);
    return union_bounds == work_area_bounds();
  }

  void ClickKebabButtonToShowExpandedMenu() {
    LeftClickOn(kebab_button());
    EXPECT_TRUE(snap_group_expanded_menu_widget());
    EXPECT_TRUE(snap_group_expanded_menu_view());
  }

  // Clicks on the swap windows button in the expanded menu and verify that the
  // windows are swapped to their oppsite position together with updating their
  // bounds. After the swap operation is completed, the union bounds of the
  // windows and split view divider will be equal to the work area bounds.
  void ClickSwapWindowsButtonAndVerify() {
    EXPECT_TRUE(snap_group_expanded_menu_widget());
    EXPECT_TRUE(snap_group_expanded_menu_view());

    const auto* cached_primary_window =
        split_view_controller()->primary_window();
    const auto* cached_secondary_window =
        split_view_controller()->secondary_window();

    IconButton* swap_button = swap_windows_button();
    EXPECT_TRUE(swap_button);
    LeftClickOn(swap_button);
    auto* new_primary_window = split_view_controller()->primary_window();
    auto* new_secondary_window = split_view_controller()->secondary_window();
    EXPECT_TRUE(SnapGroupController::Get()->AreWindowsInSnapGroup(
        new_primary_window, new_secondary_window));
    EXPECT_EQ(new_primary_window, cached_secondary_window);
    EXPECT_EQ(new_secondary_window, cached_primary_window);
    EXPECT_TRUE(UnionBoundsEqualToWorkAreaBounds(new_primary_window,
                                                 new_secondary_window));
  }

  // Clicks on the unlock button in the expanded menu and verify that the two
  // windows locked in the snap group will be unlocked. After the unlock windows
  // operation, the windows bounds will be restored to make up for the
  // previously occupied space by the split view divider so that there will be
  // no gap between the components.
  void ClickUnlockButtonAndVerify() {
    EXPECT_TRUE(snap_group_expanded_menu_widget());
    EXPECT_TRUE(snap_group_expanded_menu_view());
    IconButton* unlock_button_on_menu = unlock_button();
    VerifyLockButton(/*locked=*/true, unlock_button_on_menu);
    EXPECT_TRUE(unlock_button);
    auto* cached_primary_window = split_view_controller()->primary_window();
    auto* cached_secondary_window = split_view_controller()->secondary_window();

    LeftClickOn(unlock_button());
    EXPECT_FALSE(SnapGroupController::Get()->AreWindowsInSnapGroup(
        cached_primary_window, cached_secondary_window));
    EXPECT_TRUE(UnionBoundsEqualToWorkAreaBounds(cached_primary_window,
                                                 cached_secondary_window));
  }

  void CompleteWindowCycling() {
    WindowCycleController* window_cycle_controller =
        Shell::Get()->window_cycle_controller();
    window_cycle_controller->CompleteCycling();
    EXPECT_FALSE(window_cycle_controller->IsCycling());
  }

  void CycleWindow(WindowCyclingDirection direction, int steps) {
    WindowCycleController* window_cycle_controller =
        Shell::Get()->window_cycle_controller();
    for (int i = 0; i < steps; i++) {
      window_cycle_controller->HandleCycleWindow(direction);
      EXPECT_TRUE(window_cycle_controller->IsCycling());
    }
  }

  // TODO(michelefan): Consider put this test util in a base class or test file.
  std::unique_ptr<aura::Window> CreateTestWindowWithAppID(
      std::string app_id_key) {
    std::unique_ptr<aura::Window> window = CreateTestWindow();
    window->SetProperty(kAppIDKey, std::move(app_id_key));
    return window;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that on one window snapped in clamshell mode, the overview will be
// shown on the other side of the screen. When activating a window in overview,
// the window gets activated will be auto-snapped and the overview session will
// end. Close one window will end the split view mode.
TEST_F(SnapGroupEntryPointArm1Test, ClamshellSplitViewBasicFunctionalities) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get());
  w1.reset();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Tests the snap ratio is updated correctly when resizing the windows in a snap
// group with the split view divider.
TEST_F(SnapGroupEntryPointArm1Test, SnapRatioTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get());

  const gfx::Point hover_location =
      split_view_divider_bounds_in_screen().CenterPoint();
  split_view_divider()->StartResizeWithDivider(hover_location);
  const auto end_point =
      hover_location + gfx::Vector2d(-work_area_bounds().width() / 6, 0);
  split_view_divider()->ResizeWithDivider(end_point);
  split_view_divider()->EndResizeWithDivider(end_point);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_NEAR(0.33f, WindowState::Get(w1.get())->snap_ratio().value(),
              /*abs_error=*/0.1);
  EXPECT_NEAR(0.67f, WindowState::Get(w2.get())->snap_ratio().value(),
              /*abs_error=*/0.1);
}

// Tests that the windows in a snap group can be resized to an arbitrary
// location with the split view divider.
TEST_F(SnapGroupEntryPointArm1Test,
       ResizeWithSplitViewDividerToArbitraryLocations) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get());
  for (const int distance_delta : {-10, 6, -15}) {
    const auto w1_cached_bounds = w1.get()->GetBoundsInScreen();
    const auto w2_cached_bounds = w2.get()->GetBoundsInScreen();

    const gfx::Point hover_location =
        split_view_divider_bounds_in_screen().CenterPoint();
    split_view_divider()->StartResizeWithDivider(hover_location);
    split_view_divider()->ResizeWithDivider(hover_location +
                                            gfx::Vector2d(distance_delta, 0));
    EXPECT_TRUE(split_view_controller()->InSplitViewMode());

    EXPECT_EQ(w1_cached_bounds.width() + distance_delta,
              w1.get()->GetBoundsInScreen().width());
    EXPECT_EQ(w2_cached_bounds.width() - distance_delta,
              w2.get()->GetBoundsInScreen().width());
    EXPECT_EQ(w1.get()->GetBoundsInScreen().width() +
                  w2.get()->GetBoundsInScreen().width() +
                  kSplitviewDividerShortSideLength,
              work_area_bounds().width());
  }
}

// Tests that when snapping a snapped window to the same snapped state, the
// overview session will not be triggered. The Overview session will be
// triggered when the snapped window is being snapped to the other snapped
// state.
TEST_F(SnapGroupEntryPointArm1Test, TwoWindowsSnappedTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get());

  // Snap the primary window again as the primary window, the overview session
  // won't be triggered.
  SnapOneTestWindow(w1.get(),
                    /*state_type=*/chromeos::WindowStateType::kPrimarySnapped);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  // Snap the current primary window as the secondary window, the overview
  // session will be triggered.
  SnapOneTestWindow(
      w1.get(),
      /*state_type=*/chromeos::WindowStateType::kSecondarySnapped);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Select the other window in overview to form a snap group and exit overview.
  auto* item2 = GetOverviewItemForWindow(w2.get());
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(item2->GetTransformedBounds().CenterPoint()));
  event_generator->ClickLeftButton();
  WaitForOverviewExitAnimation();
}

// Tests that there is no crash when work area changed after snapping two
// windows with arm1. Docked mananifier is used as an example to trigger the
// work area change.
TEST_F(SnapGroupEntryPointArm1Test, WorkAreaChangeTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get());
  auto* docked_mangnifier_controller =
      Shell::Get()->docked_magnifier_controller();
  docked_mangnifier_controller->SetEnabled(/*enabled=*/true);
}

// Tests that a snap group and the split view divider will be will be
// automatically created on two windows snapped in the clamshell mode. The snap
// group will be removed together with the split view divider on destroying of
// one window in the snap group.
TEST_F(SnapGroupEntryPointArm1Test,
       AutomaticallyCreateGroupOnTwoWindowsSnappedInClamshell) {
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller);
  const auto& snap_groups = snap_group_controller->snap_groups_for_testing();
  const auto& window_to_snap_group_map =
      snap_group_controller->window_to_snap_group_map_for_testing();
  EXPECT_TRUE(snap_groups.empty());
  EXPECT_TRUE(window_to_snap_group_map.empty());

  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get());
  EXPECT_EQ(snap_groups.size(), 1u);
  EXPECT_EQ(window_to_snap_group_map.size(), 2u);

  std::unique_ptr<aura::Window> w3(CreateTestWindow());
  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(window_util::IsStackedBelow(w3.get(), w1.get()));

  w1.reset();
  EXPECT_FALSE(split_view_divider());
  EXPECT_TRUE(snap_groups.empty());
  EXPECT_TRUE(window_to_snap_group_map.empty());
}

// Tests that, after a window is snapped with overview on the other side,
// resizing overview works as expected.
TEST_F(SnapGroupEntryPointArm1Test, ResizeSplitViewOverviewAndWindow) {
  auto* snap_group_controller = SnapGroupController::Get();
  // TODO(sophiewen): Make this the default for SnapGroupEntryPointArm1Test.
  snap_group_controller->set_can_enter_overview_for_testing(
      /*can_enter_overview=*/true);

  // Snap one test window to start the snap group creation session.
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  SnapOneTestWindow(w1.get(), chromeos::WindowStateType::kPrimarySnapped);
  const gfx::Rect initial_bounds(w1->GetBoundsInScreen());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Drag the right edge of the window to resize the window and overview at the
  // same time. Test that the bounds are updated.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), w1.get());
  generator.set_current_screen_location(w1->GetBoundsInScreen().right_center());
  generator.DragMouseBy(50, 0);
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  gfx::Rect expected_window_bounds(initial_bounds);
  expected_window_bounds.set_width(initial_bounds.width() + 50);
  EXPECT_EQ(expected_window_bounds, w1->GetBoundsInScreen());

  gfx::Rect expected_grid_bounds(work_area_bounds());
  expected_grid_bounds.Subtract(w1->GetBoundsInScreen());
  EXPECT_EQ(expected_grid_bounds, GetOverviewGridBounds());

  // Drag past the 2/3 divider position. Test no crash.
  generator.DragMouseBy(600, 0);
  ASSERT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(work_area_bounds(), w1->GetBoundsInScreen());
}

// Tests that the split view divider will be stacked on top of both windows in
// the snap group and that on a third window activated the split view divider
// will be stacked below the newly activated window.
TEST_F(SnapGroupEntryPointArm1Test, SplitViewDividerStackingOrderTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get());
  wm::ActivateWindow(w1.get());

  SplitViewDivider* divider = split_view_divider();
  auto* divider_widget = divider->divider_widget();
  aura::Window* divider_window = divider_widget->GetNativeWindow();
  EXPECT_TRUE(window_util::IsStackedBelow(w2.get(), w1.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(w1.get(), divider_window));
  EXPECT_TRUE(window_util::IsStackedBelow(w2.get(), divider_window));

  std::unique_ptr<aura::Window> w3(
      CreateTestWindow(gfx::Rect(100, 200, 300, 400)));
  EXPECT_TRUE(window_util::IsStackedBelow(divider_window, w3.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(w1.get(), divider_window));
  EXPECT_TRUE(window_util::IsStackedBelow(w2.get(), w1.get()));

  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(window_util::IsStackedBelow(w3.get(), w1.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(w1.get(), w2.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(w2.get(), divider_window));
}

// Tests that the union bounds of the primary window, secondary window in a snap
// group and the split view divider will be equal to the work area bounds both
// in horizontal and vertical split view mode.
TEST_F(SnapGroupEntryPointArm1Test, SplitViewDividerBoundsTest) {
  for (const auto is_display_horizontal_layout : {true, false}) {
    // Need to explicitly create two windows otherwise to snap a snapped window
    // on the same position won't trigger the overview session.
    std::unique_ptr<aura::Window> w1(CreateTestWindow());
    std::unique_ptr<aura::Window> w2(CreateTestWindow());
    SnapTwoTestWindowsInArm1(w1.get(), w2.get(), is_display_horizontal_layout);
    EXPECT_TRUE(UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get()));
  }
}

TEST_F(SnapGroupEntryPointArm1Test, OverviewEnterExitBasic) {
  UpdateDisplay("800x600");

  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get(), /*horizontal=*/true);

  // Verify that full overview session is expected when starting overview from
  // accelerator and that split view divider will not be available.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview(OverviewStartAction::kAccelerator);
  WaitForOverviewEnterAnimation();
  EXPECT_TRUE(overview_controller->overview_session());
  EXPECT_EQ(GetOverviewGridBounds(), work_area_bounds());
  EXPECT_FALSE(split_view_divider());

  // Verify that the snap group is restored with two windows snapped and that
  // the split view divider becomes available on overview exit.
  ToggleOverview();
  EXPECT_FALSE(overview_controller->overview_session());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  EXPECT_TRUE(split_view_divider());
}

// Tests that partial overview is shown on the other side of the screen on one
// window snapped.
TEST_F(SnapGroupEntryPointArm1Test, PartialOverview) {
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());

  for (const auto& snap_state :
       {chromeos::WindowStateType::kPrimarySnapped,
        chromeos::WindowStateType::kSecondarySnapped}) {
    SnapOneTestWindow(w1.get(), snap_state);
    WaitForOverviewEnterAnimation();
    OverviewController* overview_controller =
        Shell::Get()->overview_controller();
    EXPECT_TRUE(overview_controller->overview_session());
    EXPECT_NE(GetOverviewGridBounds(), work_area_bounds());
    EXPECT_NEAR(GetOverviewGridBounds().width(),
                work_area_bounds().width() / 2.f,
                kSplitviewDividerShortSideLength / 2.f);
  }
}

// Tests that the overview session will not show on the other side of the
// screen on one window snapped if the overview is empty.
// TODO(b/287514790) : Re-enable the test after figuring out a way to
// differentiate whether to show full or partial overview.
TEST_F(SnapGroupEntryPointArm1Test, DISABLED_NotShowOverviewIfEmpty) {
  for (const auto snap_state : {chromeos::WindowStateType::kPrimarySnapped,
                                chromeos::WindowStateType::kSecondarySnapped}) {
    std::unique_ptr<aura::Window> w1(CreateTestWindow());
    SnapOneTestWindow(w1.get(), snap_state);
    EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  }
}

// Tests that the hit area of the split view divider can be outside of its
// bounds with the extra insets whose value is `kSplitViewDividerExtraInset`.
TEST_F(SnapGroupEntryPointArm1Test, SplitViewDividerEnlargedHitArea) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get(), /*horizontal=*/true);

  const gfx::Point cached_divider_center_point =
      split_view_divider_bounds_in_screen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  gfx::Point hover_location =
      cached_divider_center_point -
      gfx::Vector2d(kSplitviewDividerShortSideLength / 2 +
                        kSplitViewDividerExtraInset / 2,
                    0);
  event_generator->MoveMouseTo(hover_location);
  event_generator->PressLeftButton();
  const auto move_vector = -gfx::Vector2d(50, 0);
  event_generator->MoveMouseTo(hover_location + move_vector);
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_divider_bounds_in_screen().CenterPoint(),
            cached_divider_center_point + move_vector);
}

// Tests that the snap group expanded menu with four buttons will show on mouse
// cliked on the kebab button and hide when clicking again.
TEST_F(SnapGroupEntryPointArm1Test, ExpandedMenuViewTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get(), /*horizontal=*/true);

  LeftClickOn(kebab_button());
  auto* event_generator = GetEventGenerator();
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(snap_group_expanded_menu_widget());
  EXPECT_TRUE(snap_group_expanded_menu_view());
  EXPECT_TRUE(swap_windows_button());
  EXPECT_TRUE(update_primary_window_button());
  EXPECT_TRUE(update_secondary_window_button());
  EXPECT_TRUE(unlock_button());

  event_generator->ClickLeftButton();
  EXPECT_FALSE(snap_group_expanded_menu_widget());
  EXPECT_FALSE(snap_group_expanded_menu_view());
}

// Tests that the windows in the snap group are been swapped to the opposite
// position on toggling the swap windows button in the expanded menu together
// with the window bounds update. This test also tests that after resizing the
// two windows to an arbitrary position and swap the windows again, the windows
// and their bounds will be updated correctly.
TEST_F(SnapGroupEntryPointArm1Test, SwapWindowsButtonTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get(), /*horizontal=*/true);

  ClickKebabButtonToShowExpandedMenu();
  ClickSwapWindowsButtonAndVerify();

  auto* event_generator = GetEventGenerator();
  const auto hover_location =
      split_view_divider_bounds_in_screen().CenterPoint();
  event_generator->MoveMouseTo(hover_location);
  event_generator->MoveMouseTo(hover_location + gfx::Vector2d(50, 0));
  EXPECT_TRUE(kebab_button());
  ClickKebabButtonToShowExpandedMenu();
  ClickSwapWindowsButtonAndVerify();
}

// Tests the functionalities of the update primary window button and update
// secondary window button in the expanded menu. On either of the update window
// button toggled, the overview session will show on the other side of the
// screen for user to choose an alternate window, during which time the split
// view divider will hide.
TEST_F(SnapGroupEntryPointArm1Test, UpdateWindowButtonTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  std::unique_ptr<aura::Window> w3(CreateTestWindow());
  std::unique_ptr<aura::Window> w4(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get(), /*horizontal=*/true);
  ClickKebabButtonToShowExpandedMenu();

  // Click on the `update_primary_button` and the overview session will show on
  // the other side of the screen. The split view divider will hide.
  IconButton* update_primary_button = update_primary_window_button();
  ASSERT_TRUE(update_primary_button);
  LeftClickOn(update_primary_button);
  WaitForOverviewEnterAnimation();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_NE(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_FALSE(split_view_divider());

  // Upon selecting another item in the overview session, a new snap group will
  // be formed.
  auto* item3 = GetOverviewItemForWindow(w3.get());
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(item3->GetTransformedBounds().CenterPoint()));
  event_generator->ClickLeftButton();
  WaitForOverviewExitAnimation();
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_TRUE(split_view_divider());
  EXPECT_EQ(split_view_controller()->primary_window(), w3.get());
  EXPECT_EQ(split_view_controller()->secondary_window(), w2.get());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w2.get()));

  // Similar as `update_secondary_button`. Click on the
  // `update_secondary_button` and the overview session will show on the other
  // side of the screen. The split view divider will hide.
  ClickKebabButtonToShowExpandedMenu();
  IconButton* update_secondary_button = update_secondary_window_button();
  ASSERT_TRUE(update_secondary_button);
  LeftClickOn(update_secondary_button);
  WaitForOverviewEnterAnimation();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_NE(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_FALSE(split_view_divider());

  // Do another update for the snap group by selecting another candidate from
  // the overview session and verify that the snap group has been updated.
  auto* item4 = GetOverviewItemForWindow(w4.get());
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(item4->GetTransformedBounds().CenterPoint()));
  event_generator->ClickLeftButton();
  WaitForOverviewExitAnimation();
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_TRUE(split_view_divider());
  EXPECT_EQ(split_view_controller()->primary_window(), w3.get());
  EXPECT_EQ(split_view_controller()->secondary_window(), w4.get());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w4.get()));
}

// Tests that the two windows if locked in a snap group will be unlocked
// together with bounds update by pressing on the unlock button in the expanded
// menu. The lock button will then show on the shared edge of the two unlocked
// windows on mouse hover. Two windows will be locked again by pressing on the
// lock button.
TEST_F(SnapGroupEntryPointArm1Test, UnlockAndRelockWindowsTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get(), /*horizontal=*/true);
  ClickKebabButtonToShowExpandedMenu();
  ClickUnlockButtonAndVerify();

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(w1->GetBoundsInScreen().right_center());
  auto* timer = GetShowTimer();
  EXPECT_TRUE(timer);
  EXPECT_TRUE(timer->IsRunning());
  timer->FireNow();
  EXPECT_TRUE(GetResizeWidget());
  EXPECT_TRUE(GetLockWidget());
  event_generator->MoveMouseTo(w1->GetBoundsInScreen().origin());
  // Wait until multi-window resizer hides.
  // TODO(michelefan): Add test APIs for multi-window resizer to wait until
  // resizer widget hides.
  WaitForSeconds(1);
  PressLockWidgetToLockTwoWindows(w1.get(), w2.get());
}

// Tests that the windows bounds in the snap group are updated correctly with
// union bounds always equal to the work area bounds after been swapped and
// unlocked.
TEST_F(SnapGroupEntryPointArm1Test, SwapWindowsAndUnlockTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get(), /*horizontal=*/true);
  ClickKebabButtonToShowExpandedMenu();
  ClickSwapWindowsButtonAndVerify();
  ClickKebabButtonToShowExpandedMenu();
  ClickUnlockButtonAndVerify();
}

// Tests that by toggling the keyboard shortcut 'Search + Shift + G', the two
// snapped windows can be grouped or ungrouped.
TEST_F(SnapGroupEntryPointArm1Test, UseShortcutToGroupUnGroupWindows) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get(), /*horizontal=*/true);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Press the shortcut and the windows will be ungrouped.
  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_G,
                                      ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Press the shortcut again and the windows will be grouped.
  event_generator->PressAndReleaseKey(ui::VKEY_G,
                                      ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_TRUE(split_view_divider());
}

// Tests that the windows in snap group can be toggled between been minimized
// and restored with the keyboard shortcut 'Search + Shift + D', the windows
// will be remained in a snap group through these operations.
TEST_F(SnapGroupEntryPointArm1Test, UseShortcutToMinimizeWindows) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get(), /*horizontal=*/true);

  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  // Press the shortcut first time and the windows will be minimized.
  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_D,
                                      ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(w1.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(w2.get())->IsMinimized());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Press the shortcut again and the windows will be unminimized.
  event_generator->PressAndReleaseKey(ui::VKEY_D,
                                      ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(WindowState::Get(w1.get())->IsMinimized());
  EXPECT_FALSE(WindowState::Get(w2.get())->IsMinimized());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_TRUE(split_view_divider());
}

TEST_F(SnapGroupEntryPointArm1Test,
       SkipPairingInOverviewWhenClickingEmptyArea) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());

  SnapOneTestWindow(w1.get(), chromeos::WindowStateType::kPrimarySnapped);
  WaitForOverviewEnterAnimation();
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(WindowState::Get(w1.get())->GetStateType(),
            chromeos::WindowStateType::kPrimarySnapped);
  ASSERT_EQ(1u, GetOverviewSession()->grid_list().size());

  auto* w2_overview_item = GetOverviewItemForWindow(w2.get());
  EXPECT_TRUE(w2_overview_item);
  const gfx::Point outside_point =
      gfx::ToRoundedPoint(
          w2_overview_item->GetTransformedBounds().bottom_right()) +
      gfx::Vector2d(20, 20);

  // Verify that clicking on an empty area in overview will exit the paring.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(outside_point);
  event_generator->ClickLeftButton();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(WindowState::Get(w1.get())->GetStateType(),
            chromeos::WindowStateType::kPrimarySnapped);
  EXPECT_FALSE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

TEST_F(SnapGroupEntryPointArm1Test, SkipPairingInOverviewWithEscapeKey) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());

  SnapOneTestWindow(w1.get(), chromeos::WindowStateType::kPrimarySnapped);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(WindowState::Get(w1.get())->GetStateType(),
            chromeos::WindowStateType::kPrimarySnapped);
  ASSERT_EQ(1u, GetOverviewSession()->grid_list().size());

  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(WindowState::Get(w1.get())->GetStateType(),
            chromeos::WindowStateType::kPrimarySnapped);
  EXPECT_FALSE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Tests that the lock widget will not show on the shared edge of two unsnapped
// windows.
TEST_F(SnapGroupEntryPointArm1Test,
       MultiWindowResizeControllerWithTwoUnsnappedWindows) {
  UpdateDisplay("800x700");

  // Create two unsnapped windows with shared edge, see the layout below:
  // _________________
  // |        |       |
  // |   w1   |  w2   |
  // |________|_______|
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTRIGHT);

  // Move mouse to the shared edge of two windows, the resize widget will show
  // but the lock widget will not show.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(w1->bounds().CenterPoint());
  auto* timer = GetShowTimer();
  EXPECT_TRUE(timer);
  EXPECT_TRUE(timer->IsRunning());
  timer->FireNow();
  EXPECT_TRUE(GetResizeWidget());
  EXPECT_FALSE(GetLockWidget());
}

// Tests that when disallowing showing overview in clamshell with `kSnapGroup`
// arm1 enabled, the overview will not show on one window snapped. The overview
// will show when re-enabling showing overview.
TEST_F(SnapGroupEntryPointArm1Test, SnapWithoutShowingOverview) {
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  snap_group_controller->set_can_enter_overview_for_testing(
      /*can_enter_overview=*/false);

  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  std::unique_ptr<aura::Window> w3(CreateTestWindow());
  SnapOneTestWindow(w1.get(), chromeos::WindowStateType::kPrimarySnapped);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  SnapOneTestWindow(w2.get(), chromeos::WindowStateType::kSecondarySnapped);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  w2.reset();

  snap_group_controller->set_can_enter_overview_for_testing(
      /*can_enter_overview=*/true);
  SnapOneTestWindow(w1.get(), chromeos::WindowStateType::kSecondarySnapped);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that the window list is reordered when there is snap group. The two
// windows will be adjacent with each other with primary snapped window put
// before secondary snapped window.
TEST_F(SnapGroupEntryPointArm1Test, WindowReorderInAltTab) {
  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  SnapTwoTestWindowsInArm1(window0.get(), window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  wm::ActivateWindow(window2.get());
  // Initial window activation order: window2, [window1, window0].
  ASSERT_TRUE(wm::IsActiveWindow(window2.get()));

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/1);

  const auto& windows =
      window_cycle_controller->window_cycle_list()->windows_for_testing();

  // Test that the two windows in a snap group are reordered to be adjacent
  // with each other to reflect the window layout with the revised order as :
  // window2, [window0, window1].
  ASSERT_EQ(windows.size(), 3u);
  EXPECT_EQ(windows.at(0), window2.get());
  EXPECT_EQ(windows.at(1), window0.get());
  EXPECT_EQ(windows.at(2), window1.get());
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // With the activation of `window1`, `window0` will be inserted right before
  // `window1`.
  // The new window cycle list order as: [window0, window1], window2. Cycle
  // twice to focus on `window2`.
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/2);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests that the number of views to be cycled through inside the mirror
// container view of window cycle view will be the number of free-form windows
// plus snap groups.
TEST_F(SnapGroupEntryPointArm1Test, WindowCycleViewTest) {
  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  SnapTwoTestWindowsInArm1(window0.get(), window1.get());

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/3);
  const auto* window_cycle_list = window_cycle_controller->window_cycle_list();
  const auto& windows = window_cycle_list->windows_for_testing();
  EXPECT_EQ(windows.size(), 3u);

  const WindowCycleView* cycle_view = window_cycle_list->cycle_view();
  ASSERT_TRUE(cycle_view);
  EXPECT_EQ(cycle_view->mirror_container_for_testing()->children().size(), 2u);
  CompleteWindowCycling();
}

// Tests that on window that belongs to a snap group destroying while cycling
// the window list with Alt + Tab, there will be no crash. The corresponding
// child mini view hosted by the group container view will be destroyed, the
// group container view will host the other child mini view.
TEST_F(SnapGroupEntryPointArm1Test, WindowInSnapGroupDestructionInAltTab) {
  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  SnapTwoTestWindowsInArm1(window0.get(), window1.get());

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/3);
  const auto* window_cycle_list = window_cycle_controller->window_cycle_list();
  const auto& windows = window_cycle_list->windows_for_testing();
  EXPECT_EQ(windows.size(), 3u);

  const WindowCycleView* cycle_view = window_cycle_list->cycle_view();
  ASSERT_TRUE(cycle_view);
  // Verify that the number of child views hosted by mirror container is two at
  // the beginning.
  EXPECT_EQ(cycle_view->mirror_container_for_testing()->children().size(), 2u);

  // Destroy `window0` which belongs to a snap group.
  window0.reset();
  // Verify that we should still be cycling.
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  const auto* updated_window_cycle_list =
      window_cycle_controller->window_cycle_list();
  const auto& updated_windows =
      updated_window_cycle_list->windows_for_testing();
  // Verify that the updated windows list size decreased.
  EXPECT_EQ(updated_windows.size(), 2u);

  // Verify that the number of child views hosted by mirror container will still
  // be two.
  EXPECT_EQ(cycle_view->mirror_container_for_testing()->children().size(), 2u);
}

// Tests and verifies the steps it takes to focus on a window cycle item by
// tabbing and reverse tabbing. The focused item will be activated upon
// completion of window cycling.
TEST_F(SnapGroupEntryPointArm1Test, SteppingInWindowCycleView) {
  std::unique_ptr<aura::Window> window3 =
      CreateAppWindow(gfx::Rect(300, 300), AppType::CHROME_APP);
  std::unique_ptr<aura::Window> window2 =
      CreateAppWindow(gfx::Rect(200, 200), AppType::CHROME_APP);
  std::unique_ptr<aura::Window> window1 =
      CreateAppWindow(gfx::Rect(100, 100), AppType::BROWSER);
  std::unique_ptr<aura::Window> window0 =
      CreateAppWindow(gfx::Rect(10, 10), AppType::BROWSER);

  SnapTwoTestWindowsInArm1(window0.get(), window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  WindowState::Get(window3.get())->Activate();
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));

  // Window cycle list:
  // window3, [window0, window1], window2
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/2);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Window cycle list:
  // window2, window3, [window0, window1]
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/1);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));

  // Window cycle list:
  // window3, window2, [window0, window1]
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/2);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Window cycle list:
  // [window0, window1], window3, window2
  CycleWindow(WindowCyclingDirection::kBackward, /*steps=*/1);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests that the exposed rounded corners of the cycling items are rounded
// corners. The visuals will be refreshed on window destruction that belongs to
// a snap group.
TEST_F(SnapGroupEntryPointArm1Test, WindowCycleRoundedCorners) {
  std::unique_ptr<aura::Window> window0 =
      CreateAppWindow(gfx::Rect(100, 200), AppType::BROWSER);
  std::unique_ptr<aura::Window> window1 =
      CreateAppWindow(gfx::Rect(200, 300), AppType::BROWSER);
  std::unique_ptr<aura::Window> window2 =
      CreateAppWindow(gfx::Rect(300, 400), AppType::BROWSER);
  SnapTwoTestWindowsInArm1(window0.get(), window1.get());

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/3);
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  const auto* window_cycle_list = window_cycle_controller->window_cycle_list();
  const auto* cycle_view = window_cycle_list->cycle_view();
  auto& cycle_item_views = cycle_view->cycle_views_for_testing();
  ASSERT_EQ(cycle_item_views.size(), 2u);
  for (auto* cycle_item_view : cycle_item_views) {
    EXPECT_EQ(cycle_item_view->GetRoundedCorners(),
              gfx::RoundedCornersF(kWindowMiniViewCornerRadius));
  }

  // Destroy `window0` which belongs to a snap group.
  window0.reset();
  auto& new_cycle_item_views = cycle_view->cycle_views_for_testing();
  EXPECT_EQ(new_cycle_item_views.size(), 2u);

  // Verify that the visuals of the cycling items will be refreshed so that the
  // exposed corners will be rounded corners.
  for (auto* cycle_item_view : new_cycle_item_views) {
    EXPECT_EQ(cycle_item_view->GetRoundedCorners(),
              gfx::RoundedCornersF(kWindowMiniViewCornerRadius));
  }
  CompleteWindowCycling();
}

// Tests that the window cycle view will not show with only one snap group
// available which is the same behavior pattern as free-form window cycling.
TEST_F(SnapGroupEntryPointArm1Test, NoWindowCycleViewWithOneSnapGroup) {
  std::unique_ptr<aura::Window> window0 = CreateAppWindow(gfx::Rect(100, 200));
  std::unique_ptr<aura::Window> window1 = CreateAppWindow(gfx::Rect(200, 300));
  SnapTwoTestWindowsInArm1(window0.get(), window1.get());

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/2);
  EXPECT_TRUE(window_cycle_controller->IsCycling());

  const auto* cycle_view =
      window_cycle_controller->window_cycle_list()->cycle_view();
  EXPECT_FALSE(cycle_view);
}

// Tests that two windows in a snap group is allowed to be shown as group item
// view only if both of them belong to the same app as the mru window. If only
// one window belongs to the app, the representation of the window will be shown
// as the individual window cycle item view.
TEST_F(SnapGroupEntryPointArm1Test, SameAppWindowCycle) {
  struct app_id_pair {
    const char* trace_message;
    const std::string app_id_2;
    const std::string app_id_3;
    const size_t windows_size;
    const size_t cycle_views_count;
  } kTestCases[]{
      {/*trace_message=*/"Windows in snap group with same app id",
       /*app_id_2=*/"A", /*app_id_3=*/"A", /*windows_size=*/4u,
       /*cycle_views_count=*/3u},
      {/*trace_message=*/"Windows in snap group with different app ids",
       /*app_id_2=*/"A", /*app_id_3=*/"B", /*windows_size=*/3u,
       /*cycle_views_count=*/3u},
  };

  std::unique_ptr<aura::Window> w0(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w1(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w2(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w3(CreateTestWindowWithAppID(std::string("A")));
  SnapTwoTestWindowsInArm1(w2.get(), w3.get());
  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  for (const auto& test_case : kTestCases) {
    w2->SetProperty(kAppIDKey, std::move(test_case.app_id_2));
    w3->SetProperty(kAppIDKey, std::move(test_case.app_id_3));

    wm::ActivateWindow(w2.get());
    ASSERT_TRUE(wm::IsActiveWindow(w2.get()));

    // Simulate pressing Alt + Backtick to trigger the same app cycling.
    auto* event_generator = GetEventGenerator();
    event_generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);
    event_generator->PressAndReleaseKey(ui::VKEY_OEM_3, ui::EF_ALT_DOWN);

    const auto* window_cycle_list =
        window_cycle_controller->window_cycle_list();
    ASSERT_TRUE(window_cycle_list->same_app_only());

    // Verify the number of windows for the cycling.
    const auto& windows = window_cycle_list->windows_for_testing();
    EXPECT_EQ(windows.size(), test_case.windows_size);
    EXPECT_TRUE(window_cycle_controller->IsCycling());
    const auto* cycle_view = window_cycle_list->cycle_view();
    ASSERT_TRUE(cycle_view);

    // Verify the number of cycle views.
    auto& cycle_item_views = cycle_view->cycle_views_for_testing();
    EXPECT_EQ(cycle_item_views.size(), test_case.cycle_views_count);
    event_generator->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
  }
}

// Tests and verifies that if one of the window in a snap group gets destroyed
// while doing same app window cycling the corresponding window cycle item view
// will be properly removed and re-configured with no crash.
TEST_F(SnapGroupEntryPointArm1Test, WindowDestructionDuringSameAppWindowCycle) {
  std::unique_ptr<aura::Window> w0(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w1(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w2(CreateTestWindowWithAppID(std::string("A")));
  SnapTwoTestWindowsInArm1(w0.get(), w1.get());

  // Simulate pressing Alt + Backtick to trigger the same app cycling.
  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);
  event_generator->PressAndReleaseKey(ui::VKEY_OEM_3, ui::EF_ALT_DOWN);

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  const auto* window_cycle_list = window_cycle_controller->window_cycle_list();
  ASSERT_TRUE(window_cycle_list->same_app_only());
  const auto* cycle_view = window_cycle_list->cycle_view();
  ASSERT_TRUE(cycle_view);
  const auto& windows = window_cycle_list->windows_for_testing();
  EXPECT_EQ(windows.size(), 3u);
  w0.reset();

  // After the window destruction, the window cycle view is still available.
  ASSERT_TRUE(cycle_view);
  const auto& updated_windows = window_cycle_list->windows_for_testing();
  EXPECT_EQ(updated_windows.size(), 2u);
  CompleteWindowCycling();
}

// Tests that if a snap group is at the beginning of a window cycling list, the
// mru window will depend on the mru window between the two windows in the snap
// group, since the windows are reordered so that it reflects the actual window
// layout.
TEST_F(SnapGroupEntryPointArm1Test, MruWindowForSameApp) {
  // Generate 5 windows with 3 of them from app A and 2 of them from app B.
  std::unique_ptr<aura::Window> w0(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w1(CreateTestWindowWithAppID(std::string("B")));
  std::unique_ptr<aura::Window> w2(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w3(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w4(CreateTestWindowWithAppID(std::string("B")));
  SnapTwoTestWindowsInArm1(w0.get(), w1.get());

  // Specifically activate the secondary snapped window with app type B.
  wm::ActivateWindow(w1.get());

  // Simulate pressing Alt + Backtick to trigger the same app cycling.
  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);
  event_generator->PressAndReleaseKey(ui::VKEY_OEM_3, ui::EF_ALT_DOWN);

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  const auto* window_cycle_list = window_cycle_controller->window_cycle_list();
  ASSERT_TRUE(window_cycle_list->same_app_only());
  const auto& windows = window_cycle_list->windows_for_testing();

  // Verify that the windows in the list that are been cycled all belong to app
  // B.
  EXPECT_EQ(windows.size(), 2u);
  CompleteWindowCycling();
}

// Tests that after creating a snap group in clamshell, transition to tablet
// mode won't crash (b/288179725).
TEST_F(SnapGroupEntryPointArm1Test, NoCrashWhenRemovingGroupInTabletMode) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get(), /*horizontal=*/true);

  SwitchToTabletMode();

  // Close w2. Test that the group is destroyed but we are still in split view.
  w2.reset();
  SnapGroupController* snap_group_controller =
      Shell::Get()->snap_group_controller();
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w1.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w2.get()));
  EXPECT_EQ(split_view_controller()->primary_window(), w1.get());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that one snap group in clamshell will be converted to windows in tablet
// split view. When converted back to clamshell, the snap group will be
// restored.
TEST_F(SnapGroupEntryPointArm1Test, ClamshellTabletTransitionWithOneSnapGroup) {
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  SnapTwoTestWindowsInArm1(window1.get(), window2.get(), /*horizontal=*/true);
  EXPECT_TRUE(split_view_divider());

  SwitchToTabletMode();
  EXPECT_TRUE(split_view_divider());
  EXPECT_EQ(0.5f, *WindowState::Get(window1.get())->snap_ratio());
  EXPECT_EQ(0.5f, *WindowState::Get(window2.get())->snap_ratio());

  ExitTabletMode();
  EXPECT_TRUE(SnapGroupController::Get()->AreWindowsInSnapGroup(window1.get(),
                                                                window2.get()));
  EXPECT_EQ(0.5f, *WindowState::Get(window1.get())->snap_ratio());
  EXPECT_EQ(0.5f, *WindowState::Get(window2.get())->snap_ratio());
  EXPECT_TRUE(split_view_divider());
}

// Tests that when converting to tablet mode with split view divider at an
// arbitrary location, the bounds of the two windows and the divider will be
// updated such that the snap ratio of the layout is one of the fixed snap
// ratios.
TEST_F(SnapGroupEntryPointArm1Test,
       ClamshellTabletTransitionGetClosestFixedRatio) {
  UpdateDisplay("900x600");
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  SnapTwoTestWindowsInArm1(window1.get(), window2.get(), /*horizontal=*/true);
  ASSERT_TRUE(split_view_divider());
  EXPECT_EQ(*WindowState::Get(window1.get())->snap_ratio(),
            chromeos::kDefaultSnapRatio);

  // Build test cases to be used for divider dragging, with expected fixed ratio
  // and corresponding pixels shown in the ASCII diagram below:
  //   ┌────────────────┬────────────────┐
  //   │                │                │
  //   │                │                │
  //   │                │                │
  //   │                │                │
  //   │                │                │
  //   │                │                │
  //   │                │                │
  //   └─────────┬──────┼───────┬────────┘
  //             │      │       │
  // ratio:     1/3    1/2     2/3
  // pixel:     300    450     600      900

  struct {
    int distance_delta;
    float expected_snap_ratio;
  } kTestCases[]{{/*distance_delta=*/-200, chromeos::kOneThirdSnapRatio},
                 {/*distance_delta=*/400, chromeos::kTwoThirdSnapRatio},
                 {/*distance_delta=*/-180, chromeos::kDefaultSnapRatio}};

  auto* event_generator = GetEventGenerator();
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          split_view_controller()->root_window()->GetChildById(
              desks_util::GetActiveDeskContainerId()));
  for (const auto test_case : kTestCases) {
    event_generator->set_current_screen_location(
        split_view_divider_bounds_in_screen().CenterPoint());
    event_generator->DragMouseBy(test_case.distance_delta, 0);
    split_view_divider()->EndResizeWithDivider(
        event_generator->current_screen_location());
    SwitchToTabletMode();
    const auto current_divider_position =
        split_view_divider()
            ->GetDividerBoundsInScreen(/*is_dragging=*/false)
            .x();

    // We need to take into consideration of the variation introduced by the
    // divider shorter side length when calculating using snap ratio, i.e.
    // `kSplitviewDividerShortSideLength / 2`.
    const auto expected_divider_position = std::round(
        work_area_bounds_in_screen.width() * test_case.expected_snap_ratio -
        kSplitviewDividerShortSideLength / 2);

    // Verifies that the bounds of the windows and divider are updated correctly
    // such that snap ratio in the new window layout is expected.
    EXPECT_NEAR(current_divider_position, expected_divider_position,
                /*abs_error=*/1);
    EXPECT_NEAR(float(window1->GetBoundsInScreen().width()) /
                    work_area_bounds_in_screen.width(),
                test_case.expected_snap_ratio, /*abs_error=*/1);
    ExitTabletMode();
  }
}

// Tests that the swap window source histogram is recorded correctly.
// TODO(michelefan): move this test to the snap group histogram test fixture
// when implementing the histograms for the feature.
TEST_F(SnapGroupEntryPointArm1Test, SwapWindowsSourceHistogramTest) {
  base::HistogramTester histogram_tester;
  constexpr char kHistogramName[] = "Ash.SplitView.SwapWindowSource";
  histogram_tester.ExpectBucketCount(
      kHistogramName, SplitViewController::SwapWindowsSource::kDoubleTap, 0);
  histogram_tester.ExpectBucketCount(
      kHistogramName,
      SplitViewController::SwapWindowsSource::kSnapGroupSwapWindowsButton, 0);

  for (const bool in_tablet_mode : {false, true}) {
    std::unique_ptr<aura::Window> w1(CreateTestWindow());
    std::unique_ptr<aura::Window> w2(CreateTestWindow());
    if (in_tablet_mode) {
      SwitchToTabletMode();
      ASSERT_TRUE(Shell::Get()->IsInTabletMode());
      split_view_controller()->SnapWindow(
          w1.get(), SplitViewController::SnapPosition::kPrimary);
      split_view_controller()->SnapWindow(
          w2.get(), SplitViewController::SnapPosition::kSecondary);
      ASSERT_EQ(split_view_controller()->primary_window(), w1.get());
      ASSERT_EQ(split_view_controller()->secondary_window(), w2.get());
      split_view_controller()->SwapWindows(
          SplitViewController::SwapWindowsSource::kDoubleTap);
      histogram_tester.ExpectBucketCount(
          kHistogramName, SplitViewController::SwapWindowsSource::kDoubleTap,
          1);
    } else {
      SnapTwoTestWindowsInArm1(w1.get(), w2.get(), /*horizontal=*/true);
      ClickKebabButtonToShowExpandedMenu();
      ClickSwapWindowsButtonAndVerify();
      histogram_tester.ExpectBucketCount(
          kHistogramName,
          SplitViewController::SwapWindowsSource::kSnapGroupSwapWindowsButton,
          1);
    }
  }
}

// Tests that the cursor type gets updated on mouse hovering over everywhere on
// the split view divider excluding the kebab button.
TEST_F(SnapGroupEntryPointArm1Test, CursorUpdateTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get(), /*horizontal=*/true);
  auto* divider = split_view_divider();
  ASSERT_TRUE(divider);

  auto divider_bounds = split_view_divider_bounds_in_screen();
  auto outside_point = split_view_divider_bounds_in_screen().CenterPoint();
  outside_point.Offset(-kSplitviewDividerShortSideLength * 5, 0);
  EXPECT_FALSE(divider_bounds.Contains(outside_point));

  auto* cursor_manager = Shell::Get()->cursor_manager();
  cursor_manager->SetCursor(CursorType::kPointer);

  // Test that the default cursor type when mouse is not hovered over the split
  // view divider.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(outside_point);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kNull, cursor_manager->GetCursor().type());

  // Test that the cursor changed to resize cursor while hovering over the split
  // view divider.
  const auto delta_vector = gfx::Vector2d(0, -10);
  const gfx::Point cached_hover_point =
      divider_bounds.CenterPoint() + delta_vector;
  event_generator->MoveMouseTo(cached_hover_point);
  EXPECT_EQ(CursorType::kColumnResize, cursor_manager->GetCursor().type());

  // Test that after resizing, the cursor type is still the resize cursor.
  event_generator->PressLeftButton();
  const auto move_vector = gfx::Vector2d(20, 0);
  event_generator->MoveMouseTo(cached_hover_point + move_vector);
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(CursorType::kColumnResize, cursor_manager->GetCursor().type());
  EXPECT_EQ(split_view_divider_bounds_in_screen().CenterPoint() + delta_vector,
            cached_hover_point + move_vector);

  // Test that when hovering over the kebab button, the cursor type changed back
  // to the default type.
  event_generator->MoveMouseTo(
      kebab_button()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(CursorType::kNull, cursor_manager->GetCursor().type());
}

// A test fixture that tests the user-initiated snap group entry point. This
// entry point is guarded by the feature flag `kSnapGroup` and will only be
// enabled when the feature param `kAutomaticallyLockGroup` is false.
class SnapGroupEntryPointArm2Test : public SnapGroupTest {
 public:
  SnapGroupEntryPointArm2Test() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kSnapGroup, {{"AutomaticLockGroup", "false"}});
  }
  SnapGroupEntryPointArm2Test(const SnapGroupEntryPointArm2Test&) = delete;
  SnapGroupEntryPointArm2Test& operator=(const SnapGroupEntryPointArm2Test&) =
      delete;
  ~SnapGroupEntryPointArm2Test() override = default;

  void SnapTwoTestWindows(aura::Window* primary_window,
                          aura::Window* secondary_window) {
    UpdateDisplay("800x700");

    WindowState* primary_window_state = WindowState::Get(primary_window);
    const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
    primary_window_state->OnWMEvent(&snap_primary);
    EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
              primary_window_state->GetStateType());

    WindowState* secondary_window_state = WindowState::Get(secondary_window);
    const WindowSnapWMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY);
    secondary_window_state->OnWMEvent(&snap_secondary);
    EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
              secondary_window_state->GetStateType());

    EXPECT_EQ(work_area_bounds().width() * chromeos::kDefaultSnapRatio,
              primary_window->bounds().width());
    EXPECT_EQ(work_area_bounds().width() * chromeos::kDefaultSnapRatio,
              secondary_window->bounds().width());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the lock widget will show below the resize widget when two windows
// are snapped. And the location of the lock widget will be updated on mouse
// move.
TEST_F(SnapGroupEntryPointArm2Test, LockWidgetShowAndMoveTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  EXPECT_FALSE(GetResizeWidget());
  EXPECT_FALSE(GetLockWidget());

  auto* event_generator = GetEventGenerator();
  auto hover_location = w1->bounds().right_center();
  event_generator->MoveMouseTo(hover_location);
  auto* timer = GetShowTimer();
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_TRUE(IsShowing());
  timer->FireNow();
  EXPECT_TRUE(GetResizeWidget());
  EXPECT_TRUE(GetLockWidget());

  gfx::Rect ori_resize_widget_bounds(
      GetResizeWidget()->GetWindowBoundsInScreen());
  gfx::Rect ori_lock_widget_bounds(GetLockWidget()->GetWindowBoundsInScreen());

  resize_controller()->MouseMovedOutOfHost();
  EXPECT_FALSE(timer->IsRunning());
  EXPECT_FALSE(IsShowing());

  const int x_delta = 0;
  const int y_delta = 5;
  hover_location.Offset(x_delta, y_delta);
  event_generator->MoveMouseTo(hover_location);
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_TRUE(IsShowing());
  timer->FireNow();
  EXPECT_TRUE(GetResizeWidget());
  EXPECT_TRUE(GetLockWidget());

  gfx::Rect new_resize_widget_bounds(
      GetResizeWidget()->GetWindowBoundsInScreen());
  gfx::Rect new_lock_widget_bounds(GetLockWidget()->GetWindowBoundsInScreen());

  gfx::Rect expected_resize_widget_bounds = ori_resize_widget_bounds;
  expected_resize_widget_bounds.Offset(x_delta, y_delta);
  gfx::Rect expected_lock_widget_bounds = ori_lock_widget_bounds;
  expected_lock_widget_bounds.Offset(x_delta, y_delta);
  EXPECT_EQ(expected_resize_widget_bounds, new_resize_widget_bounds);
  EXPECT_EQ(expected_lock_widget_bounds, new_lock_widget_bounds);
}

// Tests that a snap group will be created when pressed on the lock button and
// that the activation works correctly with the snap group.
TEST_F(SnapGroupEntryPointArm2Test, SnapGroupCreationTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  EXPECT_FALSE(GetLockWidget());

  PressLockWidgetToLockTwoWindows(w1.get(), w2.get());
  auto* snap_group_controller = SnapGroupController::Get();
  EXPECT_EQ(
      snap_group_controller->window_to_snap_group_map_for_testing().size(), 2u);
  EXPECT_EQ(snap_group_controller->snap_groups_for_testing().size(), 1u);

  std::unique_ptr<aura::Window> w3(CreateTestWindow());
  wm::ActivateWindow(w3.get());
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(window_util::IsStackedBelow(w3.get(), w2.get()));
}

}  // namespace ash
