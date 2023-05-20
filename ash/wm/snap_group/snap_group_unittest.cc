// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_expanded_menu_view.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_divider_view.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/multi_window_resize_controller.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

using ::ui::mojom::CursorType;

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

void WaitForSeconds(int seconds) {
  base::RunLoop loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Seconds(seconds));
  loop.Run();
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
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kSnapGroup, {{"AutomaticLockGroup", "false"}});
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
    auto* snap_group_controller = Shell::Get()->snap_group_controller();
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
  raw_ptr<MultiWindowResizeController, ExperimentalAsh> resize_controller_;
};

// Tests that the corresponding snap group will be created when calling
// `AddSnapGroup` and removed when calling `RemoveSnapGroup`.
TEST_F(SnapGroupTest, AddAndRemoveSnapGroupTest) {
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
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
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
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
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
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

    // The `window2` gets selected in the overview will be snapped to the
    // non-occupied snap position and the overview session will end.
    OverviewItem* item2 = GetOverviewItemForWindow(window2);
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        gfx::ToRoundedPoint(item2->GetTransformedBounds().CenterPoint()));
    event_generator->ClickLeftButton();
    WaitForOverviewExitAnimation();
    EXPECT_EQ(split_view_controller()->secondary_window(), window2);

    auto* snap_group_controller = Shell::Get()->snap_group_controller();
    ASSERT_TRUE(snap_group_controller);
    EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(window1, window2));
    EXPECT_EQ(split_view_controller()->state(),
              SplitViewController::State::kBothSnapped);

    // The split view divider and kebab button will show on two windows snapped.
    EXPECT_TRUE(split_view_divider());
    EXPECT_TRUE(kebab_button());
    EXPECT_EQ(0.5f, *WindowState::Get(window1)->snap_ratio());
    EXPECT_EQ(0.5f, *WindowState::Get(window2)->snap_ratio());
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
    EXPECT_TRUE(Shell::Get()->snap_group_controller()->AreWindowsInSnapGroup(
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
    EXPECT_FALSE(Shell::Get()->snap_group_controller()->AreWindowsInSnapGroup(
        cached_primary_window, cached_secondary_window));
    EXPECT_TRUE(UnionBoundsEqualToWorkAreaBounds(cached_primary_window,
                                                 cached_secondary_window));
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
  split_view_controller()->StartResizeWithDivider(hover_location);
  const auto end_point =
      hover_location + gfx::Vector2d(-work_area_bounds().width() / 6, 0);
  split_view_controller()->ResizeWithDivider(end_point);
  split_view_controller()->EndResizeWithDivider(end_point);
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
    split_view_controller()->StartResizeWithDivider(hover_location);
    split_view_controller()->ResizeWithDivider(
        hover_location + gfx::Vector2d(distance_delta, 0));
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
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
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

// Tests that the overview session will not show on the other side of the
// screen on one window snapped if the overview is empty.
TEST_F(SnapGroupEntryPointArm1Test, NotShowOverviewIfEmpty) {
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
  OverviewItem* item3 = GetOverviewItemForWindow(w3.get());
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
  SnapGroupController* snap_group_controller =
      Shell::Get()->snap_group_controller();
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
  OverviewItem* item4 = GetOverviewItemForWindow(w4.get());
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
  SnapGroupController* snap_group_controller =
      Shell::Get()->snap_group_controller();
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

// Tests that the windows in snap group can be minimized together with the
// keyboard shortcut 'Search + Shift + D'.
TEST_F(SnapGroupEntryPointArm1Test, UseShortcutToMinimizeWindows) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindowsInArm1(w1.get(), w2.get(), /*horizontal=*/true);

  // Press the shortcut first time and the windows will be minimized.
  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_D,
                                      ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(w1.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(w2.get())->IsMinimized());

  // Press the shortcut again and the windows state remain the same.
  event_generator->PressAndReleaseKey(ui::VKEY_D,
                                      ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(w1.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(w2.get())->IsMinimized());
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

    // TODO(b/276992238): add the snap ratio check back after the calculation is
    // fixed.
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
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
  EXPECT_EQ(
      snap_group_controller->window_to_snap_group_map_for_testing().size(), 2u);
  EXPECT_EQ(snap_group_controller->snap_groups_for_testing().size(), 1u);

  std::unique_ptr<aura::Window> w3(CreateTestWindow());
  wm::ActivateWindow(w3.get());
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(window_util::IsStackedBelow(w3.get(), w2.get()));
}

}  // namespace ash
