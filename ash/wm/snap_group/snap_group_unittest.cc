// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_lock_button.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/multi_window_resize_controller.h"
#include "ash/wm/workspace/workspace_event_handler_test_helper.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class SnapGroupTest : public AshTestBase {
 public:
  SnapGroupTest() = default;
  SnapGroupTest(const SnapGroupTest&) = delete;
  SnapGroupTest& operator=(const SnapGroupTest&) = delete;
  ~SnapGroupTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSnapGroup);
    AshTestBase::SetUp();
  }

  void SnapTwoTestWindows(aura::Window* primary_window,
                          aura::Window* secondary_window) {
    UpdateDisplay("800x700");

    WindowState* primary_window_state = WindowState::Get(primary_window);
    const WMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
    primary_window_state->OnWMEvent(&snap_primary);
    EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
              primary_window_state->GetStateType());

    WindowState* secondary_window_state = WindowState::Get(secondary_window);
    const WMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY);
    secondary_window_state->OnWMEvent(&snap_secondary);
    EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
              secondary_window_state->GetStateType());

    EXPECT_EQ(0.5f, *primary_window_state->snap_ratio());
    EXPECT_EQ(0.5f, *secondary_window_state->snap_ratio());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the corresponding snap group will be created when calling
// `AddSnapGroup` and removed when calling `RemoveSnapGroup`.
TEST_F(SnapGroupTest, AddAndRemoveSnapGroupTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  std::unique_ptr<aura::Window> w3(CreateTestWindow());

  SnapTwoTestWindows(w1.get(), w2.get());
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
  ASSERT_TRUE(snap_group_controller->AddSnapGroup(w1.get(), w2.get()));
  ASSERT_FALSE(snap_group_controller->AddSnapGroup(w1.get(), w3.get()));

  const auto& snap_groups = snap_group_controller->snap_groups_for_testing();
  const auto& window_to_snap_group_map =
      snap_group_controller->window_to_snap_group_map_for_testing();
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
  SnapTwoTestWindows(w1.get(), w2.get());
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

  SnapTwoTestWindows(w1.get(), w2.get());
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
  EXPECT_TRUE(IsStackedBelow(w3.get(), w2.get()));
}

// A test fixture that tests the user-initiated snap group entry point. This
// entry point is guarded by the feature flag `kSnapGroup` and will only be
// enabled when the feature param `kAutomaticallyLockGroup` is false.
class SnapGroupEntryPointArm2Test : public SnapGroupTest {
 public:
  SnapGroupEntryPointArm2Test() = default;
  SnapGroupEntryPointArm2Test(const SnapGroupEntryPointArm2Test&) = delete;
  SnapGroupEntryPointArm2Test& operator=(const SnapGroupEntryPointArm2Test&) =
      delete;
  ~SnapGroupEntryPointArm2Test() override = default;

  // SnapGroupTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kSnapGroup, {{"AutomaticLockGroup", "false"}});
    AshTestBase::SetUp();
    WorkspaceEventHandler* event_handler =
        WorkspaceControllerTestApi(ShellTestApi().workspace_controller())
            .GetEventHandler();
    resize_controller_ =
        WorkspaceEventHandlerTestHelper(event_handler).resize_controller();
  }

  views::Widget* GetLockWidget() const {
    return resize_controller_->lock_widget_.get();
  }

  views::Widget* GetResizeWidget() const {
    return resize_controller_->resize_widget_.get();
  }

  base::OneShotTimer* GetShowTimer() const {
    return &resize_controller_->show_timer_;
  }

  bool IsShowing() const { return resize_controller_->IsShowing(); }

  MultiWindowResizeController* resize_controller() const {
    return resize_controller_;
  }

  // Verifies that the given two windows can be locked properly and the tooltip
  // is updated accordingly.
  void ToggleLockWidgetToLockTwoWindows(aura::Window* window1,
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

    gfx::Rect lock_widget_bounds(GetLockWidget()->GetWindowBoundsInScreen());
    hover_location = lock_widget_bounds.CenterPoint();
    event_generator->MoveMouseTo(hover_location);
    EXPECT_TRUE(GetLockWidget());
    event_generator->PressLeftButton();
    event_generator->ReleaseLeftButton();
    EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(window1, window2));
    VerifyLockButton(/*locked=*/true,
                     resize_controller_->lock_button_for_testing());
  }

  // Verifies that the given two windows can be unlocked properly and the
  // tooltip is updated accordingly.
  void ToggleLockWidgetToUnlockTwoWindows(aura::Window* window1,
                                          aura::Window* window2) {
    auto* snap_group_controller = Shell::Get()->snap_group_controller();
    ASSERT_TRUE(snap_group_controller);
    EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(window1, window2));

    auto* event_generator = GetEventGenerator();
    const auto hover_location =
        GetLockWidget()->GetWindowBoundsInScreen().CenterPoint();
    event_generator->MoveMouseTo(hover_location);
    EXPECT_TRUE(GetLockWidget());
    event_generator->PressLeftButton();
    event_generator->ReleaseLeftButton();
    EXPECT_FALSE(
        snap_group_controller->AreWindowsInSnapGroup(window1, window2));
    VerifyLockButton(/*locked=*/false,
                     resize_controller_->lock_button_for_testing());
  }

 private:
  // Verifies that the icon image and the tooltip of the lock button gets
  // updated correctly based on the `locked` state.
  void VerifyLockButton(bool locked, SnapGroupLockButton* lock_button) {
    SkColor color =
        lock_button->GetColorProvider()->GetColor(kColorAshIconColorPrimary);
    const gfx::ImageSkia locked_icon_image =
        gfx::CreateVectorIcon(kLockScreenEasyUnlockCloseIcon, color);
    const gfx::ImageSkia unlocked_icon_image =
        gfx::CreateVectorIcon(kLockScreenEasyUnlockOpenIcon, color);
    const SkBitmap* expected_icon =
        locked ? locked_icon_image.bitmap() : unlocked_icon_image.bitmap();
    const SkBitmap* actual_icon =
        lock_button->GetImage(views::ImageButton::ButtonState::STATE_NORMAL)
            .bitmap();
    EXPECT_TRUE(gfx::test::AreBitmapsEqual(*actual_icon, *expected_icon));

    const auto expected_tooltip_string = l10n_util::GetStringUTF16(
        locked ? IDS_ASH_SNAP_GROUP_CLICK_TO_UNLOCK_WINDOWS
               : IDS_ASH_SNAP_GROUP_CLICK_TO_LOCK_WINDOWS);
    EXPECT_EQ(lock_button->GetTooltipText(), expected_tooltip_string);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  MultiWindowResizeController* resize_controller_;
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

// Tests that a snap group will be created and removed by toggling the lock
// widget.
TEST_F(SnapGroupEntryPointArm2Test,
       SnapGroupAddAndRemovalThroughLockButtonTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  EXPECT_FALSE(GetLockWidget());

  auto* snap_group_controller = Shell::Get()->snap_group_controller();
  ToggleLockWidgetToLockTwoWindows(w1.get(), w2.get());
  EXPECT_EQ(
      snap_group_controller->window_to_snap_group_map_for_testing().size(), 2u);
  EXPECT_EQ(snap_group_controller->snap_groups_for_testing().size(), 1u);

  ToggleLockWidgetToUnlockTwoWindows(w1.get(), w2.get());
  EXPECT_TRUE(
      snap_group_controller->window_to_snap_group_map_for_testing().empty());
  EXPECT_TRUE(snap_group_controller->snap_groups_for_testing().empty());
}

// Tests the activation functionalities of the snap group.
TEST_F(SnapGroupEntryPointArm2Test, SnapGroupActivationTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  EXPECT_FALSE(GetLockWidget());

  ToggleLockWidgetToLockTwoWindows(w1.get(), w2.get());

  std::unique_ptr<aura::Window> w3(CreateTestWindow());
  wm::ActivateWindow(w3.get());
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(IsStackedBelow(w3.get(), w2.get()));

  ToggleLockWidgetToUnlockTwoWindows(w1.get(), w2.get());

  wm::ActivateWindow(w3.get());
  wm::ActivateWindow(w1.get());
  EXPECT_FALSE(IsStackedBelow(w3.get(), w2.get()));
}

}  // namespace ash