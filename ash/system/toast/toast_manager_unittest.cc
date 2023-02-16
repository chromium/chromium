// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_manager_impl.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/display/manager/display_manager.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char kToastShownCountHistogramName[] =
    "Ash.NotifierFramework.Toast.ShownCount";

constexpr char kToastTimeInQueueHistogramName[] =
    "Ash.NotifierFramework.Toast.TimeInQueue";

constexpr char kToastDismissedWithin2s[] =
    "Ash.NotifierFramework.Toast.Dismissed.Within2s";

constexpr char kToastDismissedWithin7s[] =
    "Ash.NotifierFramework.Toast.Dismissed.Within7s";

constexpr char kToastDismissedAfter7s[] =
    "Ash.NotifierFramework.Toast.Dismissed.After7s";

// Wait for the layer animation to be completed.
void WaitForAnimationEnded(ui::Layer* layer) {
  ui::LayerAnimationStoppedWaiter animation_waiter;
  animation_waiter.Wait(layer);

  // Force a frame then wait, ensuring there is one more frame presented after
  // animation finishes to allow animation throughput data to be passed from
  // cc to ui.
  ui::Compositor* compositor = layer->GetCompositor();
  compositor->ScheduleFullRedraw();
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(compositor));
}

// Waits for a time delta `time`.
void WaitForTimeDelta(base::TimeDelta time) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), time);
  run_loop.Run();
}

}  // namespace

namespace ash {

class ToastManagerImplTest : public AshTestBase {
 public:
  ToastManagerImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ToastManagerImplTest(const ToastManagerImplTest&) = delete;
  ToastManagerImplTest& operator=(const ToastManagerImplTest&) = delete;

  ~ToastManagerImplTest() override = default;

 private:
  void SetUp() override {
    AshTestBase::SetUp();

    manager_ = Shell::Get()->toast_manager();

    manager_->ResetSerialForTesting();
    EXPECT_EQ(0, GetToastSerial());

    // Start in the ACTIVE (logged-in) state.
    ChangeLockState(false);
  }

 protected:
  ToastManagerImpl* manager() { return manager_; }

  int GetToastSerial() { return manager_->serial_for_testing(); }

  // Some toasts can display on multiple root windows, so the caller can use
  // `root_window` to target a toast on a specific root window.
  ToastOverlay* GetCurrentOverlay(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows()) {
    return manager_->GetCurrentOverlayForTesting(root_window);
  }

  views::Widget* GetCurrentWidget(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows()) {
    ToastOverlay* overlay = GetCurrentOverlay(root_window);
    return overlay ? overlay->widget_for_testing() : nullptr;
  }

  views::LabelButton* GetDismissButton(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows()) {
    ToastOverlay* overlay = GetCurrentOverlay(root_window);
    DCHECK(overlay);
    return overlay->dismiss_button_for_testing();
  }

  std::u16string GetCurrentText(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows()) {
    ToastOverlay* overlay = GetCurrentOverlay(root_window);
    return overlay ? overlay->text_ : std::u16string();
  }

  std::u16string GetCurrentDismissText(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows()) {
    ToastOverlay* overlay = GetCurrentOverlay(root_window);
    return overlay ? overlay->dismiss_text_ : std::u16string();
  }

  void ClickDismissButton(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows()) {
    views::LabelButton* dismiss_button = GetDismissButton(root_window);
    const gfx::Point button_center =
        dismiss_button->GetBoundsInScreen().CenterPoint();
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(button_center);
    event_generator->ClickLeftButton();
  }

  std::string ShowToast(const std::string& text,
                        base::TimeDelta duration,
                        bool visible_on_lock_screen = false,
                        const ToastCatalogName catalog_name =
                            ToastCatalogName::kToastManagerUnittest) {
    std::string id = "TOAST_ID_" + base::NumberToString(serial_++);
    manager()->Show(ToastData(id, catalog_name, base::ASCIIToUTF16(text),
                              duration, visible_on_lock_screen));
    return id;
  }

  std::string ShowToastWithDismiss(
      const std::string& text,
      base::TimeDelta duration,
      const std::u16string& dismiss_text = std::u16string()) {
    std::string id = "TOAST_ID_" + base::NumberToString(serial_++);
    manager()->Show(ToastData(id, ToastCatalogName::kToastManagerUnittest,
                              base::ASCIIToUTF16(text), duration,
                              /*visible_on_lock_screen=*/false,
                              /*has_dismiss_button=*/true, dismiss_text));
    return id;
  }

  void CancelToast(const std::string& id) { manager()->Cancel(id); }

  void ReplaceToast(const std::string& id,
                    const std::string& text,
                    base::TimeDelta duration,
                    bool visible_on_lock_screen = false,
                    const ToastCatalogName catalog_name =
                        ToastCatalogName::kToastManagerUnittest) {
    manager()->Show(ToastData(id, catalog_name, base::ASCIIToUTF16(text),
                              duration, visible_on_lock_screen));
  }

  void ChangeLockState(bool lock) {
    SessionInfo info;
    info.state = lock ? session_manager::SessionState::LOCKED
                      : session_manager::SessionState::ACTIVE;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }

 private:
  ToastManagerImpl* manager_ = nullptr;
  unsigned int serial_ = 0;
};

TEST_F(ToastManagerImplTest, ShowAndCloseAutomatically) {
  ShowToast("DUMMY", base::Milliseconds(10));

  EXPECT_EQ(1, GetToastSerial());

  task_environment()->FastForwardBy(base::Milliseconds(1000));
  EXPECT_FALSE(GetCurrentOverlay());
}

TEST_F(ToastManagerImplTest, ShowAndCloseManually) {
  ShowToastWithDismiss("DUMMY", ToastData::kInfiniteDuration, u"Dismiss");

  EXPECT_EQ(1, GetToastSerial());

  EXPECT_FALSE(GetCurrentWidget()->GetLayer()->GetAnimator()->is_animating());

  ClickDismissButton();

  EXPECT_EQ(nullptr, GetCurrentOverlay());
}

// TODO(crbug.com/959781): Test is flaky.
TEST_F(ToastManagerImplTest, DISABLED_ShowAndCloseManuallyDuringAnimation) {
  ui::ScopedAnimationDurationScaleMode slow_animation_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  ShowToastWithDismiss("DUMMY", ToastData::kInfiniteDuration, u"Dismiss");
  EXPECT_TRUE(GetCurrentWidget()->GetLayer()->GetAnimator()->is_animating());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, GetToastSerial());
  EXPECT_TRUE(GetCurrentWidget()->GetLayer()->GetAnimator()->is_animating());

  // Try to close it during animation.
  ClickDismissButton();

  while (GetCurrentWidget()->GetLayer()->GetAnimator()->is_animating())
    base::RunLoop().RunUntilIdle();

  // Toast isn't closed.
  EXPECT_TRUE(GetCurrentOverlay() != nullptr);
}

// TODO(crbug.com/959781): Test is flaky.
TEST_F(ToastManagerImplTest, DISABLED_NullMessageHasNoDismissButton) {
  ShowToastWithDismiss("DUMMY", base::Milliseconds(10));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetDismissButton());
}

// TODO(crbug.com/959781): Test is flaky.
TEST_F(ToastManagerImplTest, DISABLED_QueueMessage) {
  ShowToast("DUMMY1", base::Milliseconds(10));
  ShowToast("DUMMY2", base::Milliseconds(10));
  ShowToast("DUMMY3", base::Milliseconds(10));

  EXPECT_EQ(1, GetToastSerial());
  EXPECT_EQ(u"DUMMY1", GetCurrentText());

  while (GetToastSerial() != 2)
    base::RunLoop().RunUntilIdle();

  EXPECT_EQ(u"DUMMY2", GetCurrentText());

  while (GetToastSerial() != 3)
    base::RunLoop().RunUntilIdle();

  EXPECT_EQ(u"DUMMY3", GetCurrentText());
}

TEST_F(ToastManagerImplTest, PositionWithVisibleBottomShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(
      GetPrimaryWorkAreaInsets()->user_work_area_bounds()));
  EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(), 1);

  gfx::Rect shelf_bounds = shelf->GetIdealBounds();
  EXPECT_FALSE(toast_bounds.Intersects(shelf_bounds));
  EXPECT_EQ(shelf_bounds.y() - ToastOverlay::kOffset, toast_bounds.bottom());
  EXPECT_EQ(
      root_bounds.bottom() - shelf_bounds.height() - ToastOverlay::kOffset,
      toast_bounds.bottom());
}

TEST_F(ToastManagerImplTest, PositionWithHotseatShown) {
  Shelf* shelf = GetPrimaryShelf();
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  HotseatWidget* hotseat = GetPrimaryShelf()->hotseat_widget();

  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  tablet_mode_controller->SetEnabledForTest(true);
  ShowToast("DUMMY", ToastData::kInfiniteDuration);

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect hotseat_bounds = hotseat->GetWindowBoundsInScreen();

  EXPECT_EQ(hotseat->state(), HotseatState::kShownHomeLauncher);
  EXPECT_FALSE(toast_bounds.Intersects(hotseat_bounds));
  EXPECT_EQ(hotseat->GetTargetBounds().y() -
                GetPrimaryWorkAreaInsets()->user_work_area_bounds().y() -
                ToastOverlay::kOffset,
            toast_bounds.bottom());
}

TEST_F(ToastManagerImplTest, PositionWithHotseatExtended) {
  Shelf* shelf = GetPrimaryShelf();
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  HotseatWidget* hotseat = GetPrimaryShelf()->hotseat_widget();

  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  tablet_mode_controller->SetEnabledForTest(true);
  hotseat->SetState(HotseatState::kExtended);
  ShowToast("DUMMY", ToastData::kInfiniteDuration);

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect hotseat_bounds = hotseat->GetWindowBoundsInScreen();

  EXPECT_FALSE(toast_bounds.Intersects(hotseat_bounds));
  EXPECT_EQ(GetPrimaryWorkAreaInsets()->user_work_area_bounds().height() -
                hotseat->GetHotseatSize() - ToastOverlay::kOffset,
            toast_bounds.bottom());
}

TEST_F(ToastManagerImplTest, PositionWithHotseatShownForMultipleMonitors) {
  UpdateDisplay("600x400,600x400");
  Shelf* shelf = GetPrimaryShelf();
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  HotseatWidget* hotseat = GetPrimaryShelf()->hotseat_widget();

  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  tablet_mode_controller->SetEnabledForTest(true);
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, absl::nullopt);

  ShowToast("DUMMY", ToastData::kInfiniteDuration);

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect hotseat_bounds = hotseat->GetWindowBoundsInScreen();

  EXPECT_EQ(hotseat->state(), HotseatState::kShownHomeLauncher);
  EXPECT_FALSE(toast_bounds.Intersects(hotseat_bounds));
  EXPECT_EQ(hotseat->GetTargetBounds().y() -
                GetPrimaryWorkAreaInsets()->user_work_area_bounds().y() -
                ToastOverlay::kOffset,
            toast_bounds.bottom());
}

TEST_F(ToastManagerImplTest, PositionWithHotseatExtendedOnSecondMonitor) {
  UpdateDisplay("600x400,600x400");
  Shelf* const shelf =
      Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
          ->shelf();
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  HotseatWidget* hotseat = GetPrimaryShelf()->hotseat_widget();

  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  tablet_mode_controller->SetEnabledForTest(true);
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, absl::nullopt);

  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(700, 100, 200, 200)));

  hotseat->SetState(HotseatState::kExtended);
  ShowToast("DUMMY", ToastData::kInfiniteDuration);

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect hotseat_bounds = hotseat->GetWindowBoundsInScreen();

  EXPECT_EQ(hotseat->state(), HotseatState::kExtended);
  EXPECT_FALSE(toast_bounds.Intersects(hotseat_bounds));
  EXPECT_EQ(hotseat->GetTargetBounds().y() -
                GetPrimaryWorkAreaInsets()->user_work_area_bounds().y() -
                ToastOverlay::kOffset,
            toast_bounds.bottom());
}

TEST_F(ToastManagerImplTest, PositionWithAutoHiddenBottomShelf) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));

  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(
      GetPrimaryWorkAreaInsets()->user_work_area_bounds()));
  EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(), 1);
  EXPECT_EQ(root_bounds.bottom() -
                ShelfConfig::Get()->hidden_shelf_in_screen_portion() -
                ToastOverlay::kOffset,
            toast_bounds.bottom());
}

TEST_F(ToastManagerImplTest, PositionWithHiddenBottomShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlwaysHidden);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(
      GetPrimaryWorkAreaInsets()->user_work_area_bounds()));
  EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(), 1);
  EXPECT_EQ(root_bounds.bottom() - ToastOverlay::kOffset,
            toast_bounds.bottom());
}

TEST_F(ToastManagerImplTest, PositionWithVisibleLeftShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  shelf->SetAlignment(ShelfAlignment::kLeft);

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::RectF precise_toast_bounds(toast_bounds);
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(
      GetPrimaryWorkAreaInsets()->user_work_area_bounds()));
  EXPECT_EQ(root_bounds.bottom() - ToastOverlay::kOffset,
            toast_bounds.bottom());

  gfx::Rect shelf_bounds = shelf->GetIdealBounds();
  EXPECT_FALSE(toast_bounds.Intersects(shelf_bounds));
  EXPECT_NEAR(
      shelf_bounds.right() + (root_bounds.width() - shelf_bounds.width()) / 2.0,
      precise_toast_bounds.CenterPoint().x(), 1.f /* accepted error */);
}

TEST_F(ToastManagerImplTest, PositionWithUnifiedDesktop) {
  display_manager()->SetUnifiedDesktopEnabled(true);
  UpdateDisplay("1000x500,0+600-100x500");

  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(
      GetPrimaryWorkAreaInsets()->user_work_area_bounds()));
  EXPECT_TRUE(root_bounds.Contains(toast_bounds));
  EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(), 1);

  gfx::Rect shelf_bounds = shelf->GetIdealBounds();
  EXPECT_FALSE(toast_bounds.Intersects(shelf_bounds));
  EXPECT_EQ(shelf_bounds.y() - ToastOverlay::kOffset, toast_bounds.bottom());
  EXPECT_EQ(
      root_bounds.bottom() - shelf_bounds.height() - ToastOverlay::kOffset,
      toast_bounds.bottom());
}

TEST_F(ToastManagerImplTest, CancelToast) {
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration);
  std::string id2 = ShowToast("TEXT2", ToastData::kInfiniteDuration);
  std::string id3 = ShowToast("TEXT3", ToastData::kInfiniteDuration);

  // Confirm that the first toast is shown.
  EXPECT_EQ(u"TEXT1", GetCurrentText());
  // Cancel the queued toast.
  CancelToast(id2);
  // Confirm that the shown toast is still visible.
  EXPECT_EQ(u"TEXT1", GetCurrentText());
  // Cancel the shown toast.
  CancelToast(id1);
  // Confirm that the next toast is visible.
  EXPECT_EQ(u"TEXT3", GetCurrentText());
  // Cancel the shown toast.
  CancelToast(id3);
  // Confirm that the shown toast disappears.
  EXPECT_FALSE(GetCurrentOverlay());
  // Confirm that only 1 toast is shown.
  EXPECT_EQ(2, GetToastSerial());
}

TEST_F(ToastManagerImplTest, ReplaceContentsOfQueuedToast) {
  std::string id1 = ShowToast(/*text=*/"TEXT1", ToastData::kInfiniteDuration);
  std::string id2 = ShowToast(/*text=*/"TEXT2", ToastData::kInfiniteDuration);

  // Confirm that the first toast is shown.
  EXPECT_EQ(u"TEXT1", GetCurrentText());
  EXPECT_EQ(1, GetToastSerial());

  // Replace the contents of the queued toast.
  ReplaceToast(id2, /*text=*/"TEXT2_updated", ToastData::kInfiniteDuration);

  // Confirm that the shown toast is still visible.
  EXPECT_EQ(u"TEXT1", GetCurrentText());
  EXPECT_EQ(1, GetToastSerial());

  // Cancel the shown toast.
  CancelToast(id1);

  // Confirm that the next toast is visible with the updated text.
  EXPECT_EQ(u"TEXT2_updated", GetCurrentText());
  EXPECT_EQ(2, GetToastSerial());
}

TEST_F(ToastManagerImplTest, ReplaceContentsOfCurrentToast) {
  std::string id1 = ShowToast(/*text=*/"TEXT1", ToastData::kInfiniteDuration);
  std::string id2 = ShowToast(/*text=*/"TEXT2", ToastData::kInfiniteDuration);

  // Confirm that the first toast is shown.
  EXPECT_EQ(u"TEXT1", GetCurrentText());
  EXPECT_EQ(1, GetToastSerial());

  // Replace the contents of the current toast showing.
  ReplaceToast(id1, /*text=*/"TEXT1_updated", ToastData::kInfiniteDuration);

  // Confirm that the new toast content is visible. The toast serial should be
  // different, indicating the original toast's timeout won't close the new
  // toast's.
  EXPECT_EQ(u"TEXT1_updated", GetCurrentText());
  EXPECT_EQ(2, GetToastSerial());

  // Cancel the shown toast.
  CancelToast(id1);

  // Confirm that the second toast is now showing.
  EXPECT_EQ(u"TEXT2", GetCurrentText());
  EXPECT_EQ(3, GetToastSerial());
}

TEST_F(ToastManagerImplTest,
       ReplaceContentsOfCurrentToastBeforePriorReplacementFinishes) {
  // By default, the animation duration is zero in tests. Set the animation
  // duration to non-zero so that toasts don't immediately close.
  ui::ScopedAnimationDurationScaleMode animation_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::string id1 = ShowToast(/*text=*/"TEXT1", ToastData::kInfiniteDuration);
  std::string id2 = ShowToast(/*text=*/"TEXT2", ToastData::kInfiniteDuration);

  // Confirm that the first toast is shown.
  EXPECT_EQ(u"TEXT1", GetCurrentText());
  EXPECT_EQ(1, GetToastSerial());

  // Replace the contents of the current toast showing. This will start the
  // animation to close the current toast.
  ReplaceToast(id1, /*text=*/"TEXT1_updated", ToastData::kInfiniteDuration);

  // Before the current toast's closing animation has finished, replace the
  // toast with another toast.
  ReplaceToast(id1, /*text=*/"TEXT1_updated2", ToastData::kInfiniteDuration);

  // Wait until the first toast's closing animation has finished. See
  // crbug/1347919
  WaitForAnimationEnded(GetCurrentWidget()->GetLayer());

  // Confirm that the most recent toast content is visible. The toast serial
  // should be different, indicating the original toast's timeout won't close
  // the new toast's.
  EXPECT_EQ(u"TEXT1_updated2", GetCurrentText());
  EXPECT_EQ(2, GetToastSerial());

  // Cancel the shown toast and wait for the animation to finish. See
  // crbug/1347919.
  CancelToast(id1);
  WaitForAnimationEnded(GetCurrentWidget()->GetLayer());

  // Confirm that the toast now showing corresponds with id2.
  EXPECT_EQ(u"TEXT2", GetCurrentText());
  EXPECT_EQ(3, GetToastSerial());
}

TEST_F(ToastManagerImplTest, ShowToastOnLockScreen) {
  // Simulate device lock.
  ChangeLockState(true);

  // Trying to show a toast.
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration);
  // Confirm that it's not visible because it's queued.
  EXPECT_EQ(nullptr, GetCurrentOverlay());

  // Simulate device unlock.
  ChangeLockState(false);
  EXPECT_TRUE(GetCurrentOverlay());
  EXPECT_EQ(u"TEXT1", GetCurrentText());
}

TEST_F(ToastManagerImplTest, ShowSupportedToastOnLockScreen) {
  // Simulate device lock.
  ChangeLockState(true);

  // Trying to show a toast.
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration,
                              /*visible_on_lock_screen=*/true);
  // Confirm it's visible and not queued.
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(u"TEXT1", GetCurrentText());

  // Simulate device unlock.
  ChangeLockState(false);
  // Confirm that the toast is still visible.
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(u"TEXT1", GetCurrentText());
}

TEST_F(ToastManagerImplTest, DeferToastByLockScreen) {
  // Show a toast.
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration,
                              /*visible_on_lock_screen=*/true);
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(u"TEXT1", GetCurrentText());

  // Simulate device lock.
  ChangeLockState(true);
  // Confirm that it gets hidden.
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(u"TEXT1", GetCurrentText());

  // Simulate device unlock.
  ChangeLockState(false);
  // Confirm that it gets visible again.
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(u"TEXT1", GetCurrentText());
}

TEST_F(ToastManagerImplTest, NotDeferToastForLockScreen) {
  // Show a toast.
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration,
                              /*visible_on_lock_screen=*/false);
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(u"TEXT1", GetCurrentText());

  // Simulate device lock.
  ChangeLockState(true);
  // Confirm that it gets hidden.
  EXPECT_EQ(nullptr, GetCurrentOverlay());

  // Simulate device unlock.
  ChangeLockState(false);
  // Confirm that it gets visible again.
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(u"TEXT1", GetCurrentText());
}

TEST_F(ToastManagerImplTest, DismissButton) {
  // Show a toast without dismiss button.
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration);

  // Queue a toast with custom dismiss button.
  std::string id2 =
      ShowToastWithDismiss("TEXT2", ToastData::kInfiniteDuration, u"Stop");

  // Queue a toast with default dismiss button.
  std::string id3 = ShowToastWithDismiss("TEXT3", ToastData::kInfiniteDuration);

  // Confirm that the first toast is shown.
  EXPECT_EQ(u"TEXT1", GetCurrentText());

  // Expect current toast to not have a dismiss button.
  EXPECT_EQ(std::u16string(), GetCurrentDismissText());

  // Cancel the current toast.
  CancelToast(id1);

  // Confirm that the next toast is visible.
  EXPECT_EQ(u"TEXT2", GetCurrentText());

  // Expect toast to have a dismiss button with custom text.
  EXPECT_EQ(u"Stop", GetCurrentDismissText());

  // Cancel the current toast.
  CancelToast(id2);

  // Confirm that the next toast is visible.
  EXPECT_EQ(u"TEXT3", GetCurrentText());

  // Expect toast to have a dismiss button with default text.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_TOAST_DISMISS_BUTTON),
            GetCurrentDismissText());
}

TEST_F(ToastManagerImplTest, ShownCountMetric) {
  base::HistogramTester histogram_tester;

  const ToastCatalogName catalog_name_1 = static_cast<ToastCatalogName>(1);
  const ToastCatalogName catalog_name_2 = static_cast<ToastCatalogName>(2);
  const base::TimeDelta duration = base::Seconds(2);
  constexpr char text[] = "sample text";

  // Show Toast with catalog_name_1.
  std::string id1 = ShowToast(text, duration,
                              /*visible_on_lock_screen=*/false, catalog_name_1);
  histogram_tester.ExpectBucketCount(kToastShownCountHistogramName,
                                     catalog_name_1, 1);

  // Replace existing toast a couple of times.
  ReplaceToast(id1, text, duration,
               /*visible_on_lock_screen=*/false, catalog_name_1);
  ReplaceToast(id1, text, duration,
               /*visible_on_lock_screen=*/false, catalog_name_1);
  histogram_tester.ExpectBucketCount(kToastShownCountHistogramName,
                                     catalog_name_1, 3);

  // Try to show toast with catalog_name_2 right after last toast was shown.
  ShowToast(text, duration, /*visible_on_lock_screen=*/false, catalog_name_2);

  // Fast forward the toast's duration so the queued toast is shown.
  task_environment()->FastForwardBy(duration);
  histogram_tester.ExpectBucketCount(kToastShownCountHistogramName,
                                     catalog_name_2, 1);
}

TEST_F(ToastManagerImplTest, TimeInQueueMetric) {
  base::HistogramTester histogram_tester;

  const ToastCatalogName catalog_name_1 = static_cast<ToastCatalogName>(1);
  const ToastCatalogName catalog_name_2 = static_cast<ToastCatalogName>(2);
  const base::TimeDelta duration = base::Seconds(2);
  constexpr char text[] = "sample text";

  // Show Toast with catalog_name_1.
  std::string id1 = ShowToast(text, duration, /*visible_on_lock_screen=*/false,
                              catalog_name_1);

  // 'TimeInQueue' is zero since there were no toasts in the queue.
  histogram_tester.ExpectTimeBucketCount(kToastTimeInQueueHistogramName,
                                         base::Seconds(0), 1);

  // Replace existing toast a couple of times.
  ReplaceToast(id1, text, duration,
               /*visible_on_lock_screen=*/false, catalog_name_1);
  ReplaceToast(id1, text, duration,
               /*visible_on_lock_screen=*/false, catalog_name_1);

  // 'TimeInQueue' is zero since the same toast was replaced.
  histogram_tester.ExpectTimeBucketCount(kToastTimeInQueueHistogramName,
                                         base::Seconds(0), 3);

  // Try to show toast with catalog_name_2 right after last toast was shown.
  ShowToast(text, duration, /*visible_on_lock_screen=*/false, catalog_name_2);

  // Fast forward the toast's duration so the queued toast is shown.
  task_environment()->FastForwardBy(duration);

  // 'TimeInQueue' records the toast's duration since the second toast was
  // queued right after the first one was shown.
  histogram_tester.ExpectTimeBucketCount(kToastTimeInQueueHistogramName,
                                         duration, 1);
}

TEST_F(ToastManagerImplTest, UserJourneyTimeMetric) {
  base::HistogramTester histogram_tester;

  const ToastCatalogName catalog_name = ToastCatalogName::kToastManagerUnittest;
  const base::TimeDelta duration = base::Seconds(6);
  constexpr char text[] = "sample text";

  // Show Toast and wait for it to dismiss by time-out.
  ShowToast(text, duration);
  task_environment()->FastForwardBy(duration);
  histogram_tester.ExpectBucketCount(kToastDismissedWithin7s, catalog_name, 1);

  // Show toast and replace it right after.
  std::string id = ShowToast(text, duration);
  ReplaceToast(id, text, duration);
  task_environment()->FastForwardBy(duration);

  // Replaced toast was dismissed within 2s.
  histogram_tester.ExpectBucketCount(kToastDismissedWithin2s, catalog_name, 1);
  histogram_tester.ExpectBucketCount(kToastDismissedWithin7s, catalog_name, 2);

  // Show a toast with infinite duration.
  ShowToastWithDismiss(text, ToastData::kInfiniteDuration);
  task_environment()->FastForwardBy(duration + base::Seconds(2));
  ClickDismissButton();

  // Toast with dismiss button was dismissed after 7s.
  histogram_tester.ExpectBucketCount(kToastDismissedAfter7s, catalog_name, 1);
}

// Table-driven test that checks whether a toast's expired callback is run when
// a toast is closed when the toast manager cancels the toast, when the toast
// duration cancels the toast, and when the dismiss button is pressed.
TEST_F(ToastManagerImplTest, ExpiredCallbackRunsWhenToastOverlayClosed) {
  // Covers possible ways that a toast can be cancelled.
  enum class CancellationSource {
    kToastManager,
    kDismissButton,
    kToastDuration,
  };

  struct {
    const std::string scope_trace;
    const CancellationSource source;
  } kTestCases[] = {
      {"Cancel toast through the toast manager",
       CancellationSource::kToastManager},
      {"Cancel toast by pressing the dismiss button",
       CancellationSource::kDismissButton},
      {"Cancel toast by letting duration elapse",
       CancellationSource::kToastDuration},
  };

  auto* toast_manager = manager();

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_trace);
    std::string toast_id = "TOAST_ID_" + base::NumberToString(GetToastSerial());

    // Create data for a toast that matches the test case. If the test case is
    // not `kToastDuration`, duration should be infinite, and if the test case
    // is not `kDismissButton` then we do not need a dismiss button on the
    // toast.
    ToastData toast_data(
        toast_id, ToastCatalogName::kToastManagerUnittest,
        /*text=*/u"",
        /*duration=*/test_case.source == CancellationSource::kToastDuration
            ? ToastData::kDefaultToastDuration
            : ToastData::kInfiniteDuration,
        /*visible_on_lock_screen=*/false,
        /*has_dismiss_button=*/test_case.source ==
            CancellationSource::kDismissButton);

    // Bind a lambda that will change a value to tell us whether the expired
    // callback ran.
    bool expired_callback_ran = false;
    toast_data.expired_callback = base::BindLambdaForTesting(
        [&expired_callback_ran]() { expired_callback_ran = true; });
    toast_manager->Show(std::move(toast_data));

    switch (test_case.source) {
      case CancellationSource::kToastManager: {
        toast_manager->Cancel(toast_id);
        break;
      }
      case CancellationSource::kDismissButton: {
        ClickDismissButton();
        break;
      }
      case CancellationSource::kToastDuration: {
        WaitForTimeDelta(ToastData::kDefaultToastDuration);
        break;
      }
    }

    EXPECT_TRUE(expired_callback_ran);
  }
}

// Tests that a toast that is created with `ToastData::persist_on_hover` set to
// true will not expire while the mouse is hovering over it.
TEST_F(ToastManagerImplTest, ToastsCanPersistOnHover) {
  std::string toast_id = "TOAST_ID_" + base::NumberToString(GetToastSerial());

  ToastData toast_data(toast_id, ToastCatalogName::kToastManagerUnittest,
                       /*text=*/u"");
  toast_data.persist_on_hover = true;

  auto* toast_manager = manager();
  toast_manager->Show(std::move(toast_data));
  EXPECT_TRUE(toast_manager->IsRunning(toast_id));

  // Wait for half of the toast duration to elapse.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);

  // Hover the mouse over the toast to stop the expiration countdown timer.
  views::Widget* widget = GetCurrentWidget();
  const gfx::Point toast_center =
      widget->GetNativeWindow()->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(toast_center);
  ASSERT_TRUE(widget->GetRootView()->IsMouseHovered());

  // Wait for the remainder of the default toast duration. At this point the
  // toast would normally expire, but because the mouse is hovered over it, it
  // will not.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);
  ASSERT_TRUE(toast_manager->IsRunning(toast_id));

  // Move the mouse away to resume the expiration countdown timer.
  event_generator->MoveMouseTo(gfx::Point(0, 0));
  ASSERT_FALSE(widget->GetRootView()->IsMouseHovered());

  // Wait for the toast to expire now that the toast is no longer hovered.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);
  EXPECT_FALSE(toast_manager->IsRunning(toast_id));
}

// Table-driven test that checks that toasts designated to show on all windows
// correctly show and close on all root windows.
TEST_F(ToastManagerImplTest, ShowAndCloseToastsOnAllRootWindows) {
  UpdateDisplay("800x700,800x700");

  // Covers possible ways that a toast can be cancelled.
  enum class CancellationSource {
    kToastManager,
    kDismissButton,
    kToastDuration,
  };

  struct {
    const std::string scope_trace;
    const CancellationSource source;
  } kTestCases[] = {
      {"Cancel toast through the toast manager",
       CancellationSource::kToastManager},
      {"Cancel toast by pressing the dismiss button",
       CancellationSource::kDismissButton},
      {"Cancel toast by letting duration elapse",
       CancellationSource::kToastDuration},
  };

  auto* toast_manager = manager();
  const aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_trace);
    std::string toast_id = "TOAST_ID_" + base::NumberToString(GetToastSerial());

    // Create data for a toast that matches the test case. If the test case is
    // not `kToastDuration`, duration should be infinite, and if the test case
    // is not `kDismissButton` then we do not need a dismiss button on the
    // toast.
    ToastData toast_data(
        toast_id, ToastCatalogName::kToastManagerUnittest,
        /*text=*/u"",
        /*duration=*/test_case.source == CancellationSource::kToastDuration
            ? ToastData::kDefaultToastDuration
            : ToastData::kInfiniteDuration,
        /*visible_on_lock_screen=*/false,
        /*has_dismiss_button=*/test_case.source ==
            CancellationSource::kDismissButton);

    // Indicate that the toast will show on all root windows.
    toast_data.show_on_all_root_windows = true;
    toast_manager->Show(std::move(toast_data));

    for (auto* root_window : root_windows)
      EXPECT_TRUE(GetCurrentOverlay(root_window));

    switch (test_case.source) {
      case CancellationSource::kToastManager: {
        toast_manager->Cancel(toast_id);
        break;
      }
      case CancellationSource::kDismissButton: {
        ClickDismissButton();
        break;
      }
      case CancellationSource::kToastDuration: {
        base::RunLoop run_loop;
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE, run_loop.QuitClosure(),
            ToastData::kDefaultToastDuration);
        run_loop.Run();
        break;
      }
    }

    for (auto* root_window : root_windows)
      EXPECT_FALSE(GetCurrentOverlay(root_window));
  }
}

// This tests that toasts that are designated to persist on hover and appear on
// all root windows will not close when one of the toast instances is hovered.
TEST_F(ToastManagerImplTest, ToastsThatPersistOnHoverOnAllRootWindows) {
  UpdateDisplay("800x700,800x700");
  auto* toast_manager = manager();
  const aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  std::string toast_id = "TOAST_ID_" + base::NumberToString(GetToastSerial());

  // Create a basic toast with `ToastData::kDefaultToastDuration` as duration.
  ToastData toast_data(toast_id, ToastCatalogName::kToastManagerUnittest,
                       /*text=*/u"");

  // Indicate that the toast will show on all root windows and persist on hover.
  toast_data.show_on_all_root_windows = true;
  toast_data.persist_on_hover = true;
  toast_manager->Show(std::move(toast_data));
  ASSERT_TRUE(toast_manager->IsRunning(toast_id));

  for (auto* root_window : root_windows)
    ASSERT_TRUE(GetCurrentOverlay(root_window));

  // Wait for half of the toast duration to elapse.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);

  // Hover the mouse over the toast instance on a root window (in this case the
  // default is `Shell::GetRootWindowForNewWindows()`) to stop the expiration
  // countdown timer.
  views::Widget* widget = GetCurrentWidget();
  const gfx::Point toast_center =
      widget->GetNativeWindow()->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(toast_center);
  ASSERT_TRUE(widget->GetRootView()->IsMouseHovered());

  // Wait for the other half of the toast duration to elapse. Because the mouse
  // is hovering over one of the toast instances, all toasts instances should
  // remain open after this time.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);

  for (auto* root_window : root_windows)
    EXPECT_TRUE(GetCurrentOverlay(root_window));

  // Move the mouse away to resume the expiration countdown timer.
  event_generator->MoveMouseTo(gfx::Point(0, 0));
  ASSERT_FALSE(widget->GetRootView()->IsMouseHovered());

  // Wait for the other half of the toast duration to elapse. This time, because
  // the mouse has been moved away from the toast, all toast instances should be
  // gone.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);

  for (auto* root_window : root_windows)
    EXPECT_FALSE(GetCurrentOverlay(root_window));
}

// This tests that multi-monitor toast instances do not call the
// `expired_callback_` when the root window is removed.
TEST_F(ToastManagerImplTest, ExpiredCallbackNotCalledOnRootWindowRemoved) {
  UpdateDisplay("800x700,800x700");
  auto* toast_manager = manager();

  std::string toast_id = "TOAST_ID_" + base::NumberToString(GetToastSerial());

  // Create a basic toast with `ToastData::kDefaultToastDuration` as duration.
  ToastData toast_data(toast_id, ToastCatalogName::kToastManagerUnittest,
                       /*text=*/u"");

  // Indicate that the toast will show on all root windows.
  toast_data.show_on_all_root_windows = true;

  // Bind a lambda that will change a value to tell us whether the expired
  // callback ran.
  bool expired_callback_ran = false;
  toast_data.expired_callback = base::BindLambdaForTesting(
      [&expired_callback_ran]() { expired_callback_ran = true; });
  toast_manager->Show(std::move(toast_data));
  ASSERT_TRUE(toast_manager->IsRunning(toast_id));

  for (auto* root_window : Shell::GetAllRootWindows())
    ASSERT_TRUE(GetCurrentOverlay(root_window));

  // Wait for half of the toast duration to elapse.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);

  // Remove a display to trigger the destruction of a toast overlay.
  // `expired_callback_ran` should still be false.
  UpdateDisplay("800x700");
  ASSERT_EQ(1u, Shell::GetAllRootWindows().size());
  ASSERT_TRUE(toast_manager->IsRunning(toast_id));
  EXPECT_FALSE(expired_callback_ran);

  // Wait for the other half of the toast duration to elapse.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);
  EXPECT_FALSE(toast_manager->IsRunning(toast_id));
  EXPECT_TRUE(expired_callback_ran);
}

// This tests that new instances of a multi-monitor toast are spawned with the
// correct duration and correct persisting state.
TEST_F(ToastManagerImplTest,
       AllRootWindowToastsCreatedWithCorrectDurationAndPersistState) {
  // Start with display at 800x700 to maintain cursor position when adding root
  // windows.
  UpdateDisplay("800x700");
  auto* toast_manager = manager();

  std::string toast_id = "TOAST_ID_" + base::NumberToString(GetToastSerial());

  // Create a basic toast with `ToastData::kDefaultToastDuration` as duration.
  ToastData toast_data(toast_id, ToastCatalogName::kToastManagerUnittest,
                       /*text=*/u"");

  // Indicate that the toast will show on all root windows and persist on hover.
  toast_data.show_on_all_root_windows = true;
  toast_data.persist_on_hover = true;
  toast_manager->Show(std::move(toast_data));
  ASSERT_TRUE(toast_manager->IsRunning(toast_id));

  // Wait for half of the toast duration to elapse.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);

  // Hover over the active toast instance to stop the expiration timer.
  views::Widget* widget = GetCurrentWidget();
  const gfx::Point toast_center =
      widget->GetNativeWindow()->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(toast_center);
  ASSERT_TRUE(widget->GetRootView()->IsMouseHovered());

  // Add a new root window while hovering over the initial toast instance. Both
  // toasts should still be persisting on hover.
  UpdateDisplay("800x700,800x700");
  ASSERT_TRUE(widget->GetRootView()->IsMouseHovered());

  // Wait for the remaining half of the toast duration to elapse. Neither toast
  // instance should be destroyed.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);

  for (auto* root_window : Shell::GetAllRootWindows())
    EXPECT_TRUE(GetCurrentOverlay(root_window));

  // Unhover the mouse an add a third root window.
  event_generator->MoveMouseTo(gfx::Point(0, 0));
  ASSERT_FALSE(widget->GetRootView()->IsMouseHovered());
  UpdateDisplay("800x700,800x700,800x700");

  // Wait for the remaining half of the toast duration to elapse. At this point
  // all three toast instances should be destroyed.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);
  base::RunLoop().RunUntilIdle();

  for (auto* root_window : Shell::GetAllRootWindows())
    EXPECT_FALSE(GetCurrentOverlay(root_window));
}

}  // namespace ash
