// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_manager_impl.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/system/scoped_toast_pause.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/work_area_insets.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
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
#include "ui/wm/core/window_util.h"

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

class ToastManagerImplTest : public AshTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  ToastManagerImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ToastManagerImplTest(const ToastManagerImplTest&) = delete;
  ToastManagerImplTest& operator=(const ToastManagerImplTest&) = delete;

  ~ToastManagerImplTest() override = default;

  void SetUp() override {
    // Side-aligned toasts project is launching with Notifier Collision, so we
    // use the flag `kNotifierCollision` here.
    scoped_feature_list_.InitWithFeatureState(features::kNotifierCollision,
                                              AreSideAlignedToastsEnabled());

    AshTestBase::SetUp();

    manager_ = Shell::Get()->toast_manager();

    manager_->ResetSerialForTesting();
    EXPECT_EQ(0, GetToastSerial());

    // Start in the ACTIVE (logged-in) state.
    ChangeLockState(false);
    SetShouldLockScreenAutomatically(false);
  }

  bool AreSideAlignedToastsEnabled() const { return GetParam(); }

 protected:
  ToastManagerImpl* manager() { return manager_; }

  int GetToastSerial() { return manager_->serial_for_testing(); }

  // Some toasts can display on multiple root windows, so the caller can use
  // `root_window` to target a toast on a specific root window.
  ToastOverlay* GetCurrentOverlay(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows()) {
    return manager_->GetCurrentOverlayForTesting(root_window);
  }

  gfx::Rect GetToastBounds() {
    return GetCurrentWidget()->GetWindowBoundsInScreen();
  }

  views::Widget* GetCurrentWidget(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows()) {
    ToastOverlay* overlay = GetCurrentOverlay(root_window);
    return overlay ? overlay->widget_for_testing() : nullptr;
  }

  std::u16string GetCurrentText(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows()) {
    ToastOverlay* overlay = GetCurrentOverlay(root_window);
    return overlay ? overlay->text_ : std::u16string();
  }

  void ClickDismissButton(
      aura::Window* root_window = Shell::GetRootWindowForNewWindows()) {
    views::LabelButton* dismiss_button =
        GetCurrentOverlay(root_window)->dismiss_button_for_testing();

    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        dismiss_button->GetBoundsInScreen().CenterPoint());
    event_generator->ClickLeftButton();
  }

  std::string ShowToast(const std::string& text,
                        base::TimeDelta duration,
                        bool visible_on_lock_screen = false,
                        const ToastCatalogName catalog_name =
                            ToastCatalogName::kTestCatalogName) {
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
    manager()->Show(ToastData(id, ToastCatalogName::kTestCatalogName,
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
                        ToastCatalogName::kTestCatalogName) {
    manager()->Show(ToastData(id, catalog_name, base::ASCIIToUTF16(text),
                              duration, visible_on_lock_screen));
  }

  void ChangeLockState(bool lock) {
    SessionInfo info;
    info.state = lock ? session_manager::SessionState::LOCKED
                      : session_manager::SessionState::ACTIVE;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }

  bool IsToastShown(const std::string& id) {
    return manager()->IsToastShown(id);
  }

 private:
  raw_ptr<ToastManagerImpl, DanglingUntriaged> manager_ = nullptr;
  unsigned int serial_ = 0;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ToastManagerImplTest,
                         testing::Bool() /* AreSideAlignedToastsEnabled() */);

TEST_P(ToastManagerImplTest, ShowAndCloseAutomatically) {
  // A toast with custom duration closes after its duration plus one second.
  base::TimeDelta custom_duration = base::Milliseconds(10);
  ShowToast("id", custom_duration);
  EXPECT_TRUE(GetCurrentOverlay());
  task_environment()->FastForwardBy(custom_duration + base::Seconds(1));
  EXPECT_FALSE(GetCurrentOverlay());

  // A toast with "infinite" duration closes after its duration plus one second.
  ShowToast("id", ToastData::kInfiniteDuration);
  EXPECT_TRUE(GetCurrentOverlay());
  task_environment()->FastForwardBy(ToastData::kInfiniteDuration +
                                    base::Seconds(1));
  EXPECT_FALSE(GetCurrentOverlay());
}

TEST_P(ToastManagerImplTest, ShowAndCloseManually) {
  ShowToastWithDismiss("DUMMY", ToastData::kInfiniteDuration, u"Dismiss");

  EXPECT_EQ(1, GetToastSerial());

  EXPECT_FALSE(GetCurrentWidget()->GetLayer()->GetAnimator()->is_animating());

  ClickDismissButton();

  EXPECT_EQ(nullptr, GetCurrentOverlay());
}

TEST_P(ToastManagerImplTest, ShowAndCloseManuallyDuringAnimation) {
  ui::ScopedAnimationDurationScaleMode slow_animation_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  ASSERT_TRUE(task_environment()->UsesMockTime());

  ShowToastWithDismiss("DUMMY", ToastData::kInfiniteDuration, u"Dismiss");
  EXPECT_TRUE(GetCurrentWidget()->GetLayer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::Milliseconds(10));

  EXPECT_EQ(1, GetToastSerial());
  EXPECT_TRUE(GetCurrentWidget()->GetLayer()->GetAnimator()->is_animating());

  // Try to close it during animation.
  ClickDismissButton();

  task_environment()->FastForwardBy(base::Seconds(10));
  base::RunLoop().RunUntilIdle();
}

TEST_P(ToastManagerImplTest, ShowToastWithScopedToastPause) {
  auto scoped_toast_pause = manager()->CreateScopedPause();

  // If a `ScopedToastPause` exists, the toast should not be shown.
  ShowToast("DUMMY", base::Milliseconds(10));
  EXPECT_EQ(0, GetToastSerial());
  EXPECT_FALSE(GetCurrentOverlay());

  // Even if the `ScopedToastPause` is destroyed, the toast doesn't exist.
  scoped_toast_pause.reset();
  EXPECT_EQ(0, GetToastSerial());
  EXPECT_FALSE(GetCurrentOverlay());
}

TEST_P(ToastManagerImplTest, CancelToastWithScopedToastPause) {
  ShowToast("DUMMY", base::Milliseconds(10));
  EXPECT_EQ(1, GetToastSerial());

  // Creates a `ScopedToastPause` and all toasts will be cleared immediately.
  manager()->CreateScopedPause();
  EXPECT_FALSE(GetCurrentOverlay());
}

TEST_P(ToastManagerImplTest, QueueToasts) {
  const base::TimeDelta kDelay = ToastData::kMinimumDuration;

  std::string id1 = ShowToast("TEXT1", kDelay);
  std::string id2 = ShowToast("TEXT2", kDelay);
  std::string id3 = ShowToast("TEXT3", kDelay);

  EXPECT_EQ(1, GetToastSerial());
  EXPECT_TRUE(IsToastShown(id1));

  task_environment()->FastForwardBy(kDelay);
  while (GetToastSerial() != 2) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(IsToastShown(id2));

  task_environment()->FastForwardBy(kDelay);
  while (GetToastSerial() != 3) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(IsToastShown(id3));
}

TEST_P(ToastManagerImplTest, PositionWithVisibleBottomShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetToastBounds();
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(
      GetPrimaryWorkAreaInsets()->user_work_area_bounds()));
  if (AreSideAlignedToastsEnabled()) {
    EXPECT_EQ(root_bounds.right(),
              toast_bounds.right() + ToastOverlay::kOffset);
  } else {
    EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(),
                1);
  }

  gfx::Rect shelf_bounds = shelf->GetIdealBounds();
  EXPECT_FALSE(toast_bounds.Intersects(shelf_bounds));
  EXPECT_EQ(shelf_bounds.y() - ToastOverlay::kOffset, toast_bounds.bottom());
  EXPECT_EQ(
      root_bounds.bottom() - shelf_bounds.height() - ToastOverlay::kOffset,
      toast_bounds.bottom());
}

TEST_P(ToastManagerImplTest, PositionWithHotseatShown) {
  Shelf* shelf = GetPrimaryShelf();
  HotseatWidget* hotseat = GetPrimaryShelf()->hotseat_widget();

  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ash::TabletModeControllerTestApi().EnterTabletMode();
  ShowToast("DUMMY", ToastData::kInfiniteDuration);

  gfx::Rect toast_bounds = GetToastBounds();
  gfx::Rect hotseat_bounds = hotseat->GetWindowBoundsInScreen();

  EXPECT_EQ(hotseat->state(), HotseatState::kShownHomeLauncher);
  EXPECT_FALSE(toast_bounds.Intersects(hotseat_bounds));
  EXPECT_EQ(hotseat->GetTargetBounds().y() -
                GetPrimaryWorkAreaInsets()->user_work_area_bounds().y() -
                ToastOverlay::kOffset,
            toast_bounds.bottom());
}

TEST_P(ToastManagerImplTest, PositionWithHotseatExtended) {
  Shelf* shelf = GetPrimaryShelf();
  HotseatWidget* hotseat = GetPrimaryShelf()->hotseat_widget();

  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ash::TabletModeControllerTestApi().EnterTabletMode();
  hotseat->SetState(HotseatState::kExtended);
  ShowToast("DUMMY", ToastData::kInfiniteDuration);

  gfx::Rect toast_bounds = GetToastBounds();
  gfx::Rect hotseat_bounds = hotseat->GetWindowBoundsInScreen();

  EXPECT_FALSE(toast_bounds.Intersects(hotseat_bounds));
  EXPECT_EQ(GetPrimaryWorkAreaInsets()->user_work_area_bounds().height() -
                hotseat->GetHotseatSize() - ToastOverlay::kOffset -
                ShelfConfig::Get()->hotseat_bottom_padding(),
            toast_bounds.bottom());
}

TEST_P(ToastManagerImplTest, PositionWithHotseatShownForMultipleMonitors) {
  UpdateDisplay("600x400,600x400");
  Shelf* shelf = GetPrimaryShelf();
  HotseatWidget* hotseat = GetPrimaryShelf()->hotseat_widget();

  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ash::TabletModeControllerTestApi().EnterTabletMode();
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);

  ShowToast("DUMMY", ToastData::kInfiniteDuration);

  gfx::Rect toast_bounds = GetToastBounds();
  gfx::Rect hotseat_bounds = hotseat->GetWindowBoundsInScreen();

  EXPECT_EQ(hotseat->state(), HotseatState::kShownHomeLauncher);
  EXPECT_FALSE(toast_bounds.Intersects(hotseat_bounds));
  EXPECT_EQ(hotseat->GetTargetBounds().y() -
                GetPrimaryWorkAreaInsets()->user_work_area_bounds().y() -
                ToastOverlay::kOffset,
            toast_bounds.bottom());
}

// Tests that `ToastOverlay`'s are cleaned up properly on shutdown with hotseat
// extended on multi-monitor
TEST_P(ToastManagerImplTest, ShutdownWithExtendedHotseat) {
  UpdateDisplay("600x400,600x400");
  Shelf* const shelf =
      Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
          ->shelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ash::TabletModeControllerTestApi().EnterTabletMode();
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);

  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(700, 100, 200, 200)));

  GetPrimaryShelf()->hotseat_widget()->SetState(HotseatState::kExtended);

  ShowToast("DUMMY", ToastData::kInfiniteDuration);

  // Shutdown, there should be no crash.
}

// Tests that toasts that observe UnifiedSystemTray and are shown in
// multiple displays are properly destroyed after disconnecting a monitor.
TEST_P(ToastManagerImplTest, ToastsOnMultipleMonitors) {
  UpdateDisplay("800x700,800x700");
  auto* toast_manager = manager();

  std::string toast_id = "TOAST_ID_" + base::NumberToString(GetToastSerial());

  // Create a basic toast with `ToastData::kDefaultToastDuration` as duration.
  ToastData toast_data(toast_id, ToastCatalogName::kTestCatalogName,
                       /*text=*/u"");

  // Indicate that the toast will show on all root windows.
  toast_data.show_on_all_root_windows = true;

  toast_manager->Show(std::move(toast_data));
  ASSERT_TRUE(toast_manager->IsToastShown(toast_id));
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    ASSERT_TRUE(GetCurrentOverlay(root_window));
  }

  // Wait for half of the toast duration to elapse.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);

  // Remove a display to trigger the destruction of a toast overlay.
  UpdateDisplay("800x700");
  ASSERT_EQ(1u, Shell::GetAllRootWindows().size());
  ASSERT_TRUE(toast_manager->IsToastShown(toast_id));

  // No crash should happen.
}

TEST_P(ToastManagerImplTest, PositionWithHotseatExtendedOnSecondMonitor) {
  UpdateDisplay("600x400,700x400");
  RootWindowController* const secondary_root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id());
  Shelf* const shelf = secondary_root_window_controller->shelf();
  HotseatWidget* hotseat = shelf->hotseat_widget();

  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ash::TabletModeControllerTestApi().EnterTabletMode();
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);

  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(700, 100, 200, 200)));
  shelf->hotseat_widget()->set_manually_extended(true);
  shelf->shelf_widget()->shelf_layout_manager()->UpdateVisibilityState(
      /*force_layout=*/false);

  EXPECT_EQ(hotseat->state(), HotseatState::kExtended);

  ShowToast("DUMMY", ToastData::kInfiniteDuration);

  gfx::Rect toast_bounds = GetToastBounds();
  gfx::Rect hotseat_bounds = hotseat->GetWindowBoundsInScreen();

  EXPECT_EQ(hotseat->state(), HotseatState::kExtended);
  EXPECT_FALSE(toast_bounds.Intersects(hotseat_bounds));
  EXPECT_EQ(hotseat->GetTargetBounds().y() -
                secondary_root_window_controller->work_area_insets()
                    ->user_work_area_bounds()
                    .y() -
                ToastOverlay::kOffset,
            toast_bounds.bottom());
}

TEST_P(ToastManagerImplTest, PositionWithHotseatExtendedOnAnotherMonitor) {
  UpdateDisplay("600x400,700x400");
  RootWindowController* const secondary_root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id());
  Shelf* const shelf = secondary_root_window_controller->shelf();
  HotseatWidget* hotseat = shelf->hotseat_widget();

  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ash::TabletModeControllerTestApi().EnterTabletMode();
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);

  // Create two windows, one on each display. The window creation order should
  // result in the window on the primary display being active.
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(700, 100, 200, 200)));
  std::unique_ptr<aura::Window> primary_display_window(
      CreateTestWindow(gfx::Rect(0, 100, 200, 200)));

  // Extend the hotseat on the secondary display.
  shelf->hotseat_widget()->set_manually_extended(true);
  shelf->shelf_widget()->shelf_layout_manager()->UpdateVisibilityState(
      /*force_layout=*/false);
  EXPECT_EQ(hotseat->state(), HotseatState::kExtended);

  // Show the toast - should be shown on the primary display (on the display
  // with the latest active window).
  ShowToast("DUMMY", ToastData::kInfiniteDuration);

  const gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  const gfx::Rect primary_work_area_bounds =
      GetPrimaryWorkAreaInsets()->user_work_area_bounds();

  EXPECT_TRUE(primary_work_area_bounds.Contains(toast_bounds));
  EXPECT_EQ(primary_work_area_bounds.bottom() - ToastOverlay::kOffset,
            toast_bounds.bottom());
}

TEST_P(ToastManagerImplTest, PositionWithAutoHiddenBottomShelf) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));

  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetToastBounds();
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(
      GetPrimaryWorkAreaInsets()->user_work_area_bounds()));
  if (AreSideAlignedToastsEnabled()) {
    EXPECT_EQ(root_bounds.right(),
              toast_bounds.right() + ToastOverlay::kOffset);
  } else {
    EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(),
                1);
  }
  EXPECT_EQ(root_bounds.bottom() -
                ShelfConfig::Get()->hidden_shelf_in_screen_portion() -
                ToastOverlay::kOffset,
            toast_bounds.bottom());

  // Hide the window so the shelf is shown, the toast baseline should update.
  window->Hide();
  toast_bounds = GetToastBounds();

  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(root_bounds.bottom() - ShelfConfig::Get()->shelf_size() -
                ToastOverlay::kOffset,
            toast_bounds.bottom());
}

TEST_P(ToastManagerImplTest, PositionWithHiddenBottomShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlwaysHidden);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetToastBounds();
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(
      GetPrimaryWorkAreaInsets()->user_work_area_bounds()));
  if (AreSideAlignedToastsEnabled()) {
    EXPECT_EQ(root_bounds.right(),
              toast_bounds.right() + ToastOverlay::kOffset);
  } else {
    EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(),
                1);
  }
  EXPECT_EQ(root_bounds.bottom() - ToastOverlay::kOffset,
            toast_bounds.bottom());
}

// Tests that toasts follow the shelf when aligning it to the side.
// Toasts should stay at center of the work area if side aligned toasts are not
// enabled.
TEST_P(ToastManagerImplTest, PositionWithVisibleSideShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect work_area_bounds;
  gfx::Rect shelf_bounds;

  shelf->SetAlignment(ShelfAlignment::kLeft);
  work_area_bounds = GetPrimaryWorkAreaInsets()->user_work_area_bounds();
  shelf_bounds = shelf->GetIdealBounds();
  EXPECT_FALSE(GetToastBounds().Intersects(shelf_bounds));
  if (AreSideAlignedToastsEnabled()) {
    EXPECT_EQ(work_area_bounds.x(),
              GetToastBounds().x() - ToastOverlay::kOffset);
  } else {
    EXPECT_NEAR(work_area_bounds.CenterPoint().x(),
                GetToastBounds().CenterPoint().x(), 1);
  }

  shelf->SetAlignment(ShelfAlignment::kRight);
  work_area_bounds = GetPrimaryWorkAreaInsets()->user_work_area_bounds();
  shelf_bounds = shelf->GetIdealBounds();
  EXPECT_FALSE(GetToastBounds().Intersects(shelf_bounds));
  if (AreSideAlignedToastsEnabled()) {
    EXPECT_EQ(work_area_bounds.right(),
              GetToastBounds().right() + ToastOverlay::kOffset);
  } else {
    EXPECT_NEAR(work_area_bounds.CenterPoint().x(),
                GetToastBounds().CenterPoint().x(), 1);
  }
}

TEST_P(ToastManagerImplTest, PositionWithUnifiedDesktop) {
  display_manager()->SetUnifiedDesktopEnabled(true);
  UpdateDisplay("1000x500,0+600-100x500");

  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetToastBounds();
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(
      GetPrimaryWorkAreaInsets()->user_work_area_bounds()));
  EXPECT_TRUE(root_bounds.Contains(toast_bounds));
  if (AreSideAlignedToastsEnabled()) {
    EXPECT_EQ(root_bounds.right(),
              toast_bounds.right() + ToastOverlay::kOffset);
  } else {
    EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(),
                1);
  }

  gfx::Rect shelf_bounds = shelf->GetIdealBounds();
  EXPECT_FALSE(toast_bounds.Intersects(shelf_bounds));
  EXPECT_EQ(shelf_bounds.y() - ToastOverlay::kOffset, toast_bounds.bottom());
  EXPECT_EQ(
      root_bounds.bottom() - shelf_bounds.height() - ToastOverlay::kOffset,
      toast_bounds.bottom());
}

TEST_P(ToastManagerImplTest, CancelToast) {
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration);
  std::string id2 = ShowToast("TEXT2", ToastData::kInfiniteDuration);
  std::string id3 = ShowToast("TEXT3", ToastData::kInfiniteDuration);

  // Confirm that the first toast is shown.
  EXPECT_TRUE(IsToastShown(id1));

  // Cancel the queued toast and confirm the first toast is still visible.
  CancelToast(id2);
  EXPECT_TRUE(IsToastShown(id1));
  EXPECT_FALSE(IsToastShown(id2));
  EXPECT_FALSE(IsToastShown(id3));

  // Cancel the shown toast and confirm the next toast is visible.
  CancelToast(id1);
  EXPECT_FALSE(IsToastShown(id1));
  EXPECT_FALSE(IsToastShown(id2));
  EXPECT_TRUE(IsToastShown(id3));

  // Cancel the shown toast and confirm there are no more toasts.
  CancelToast(id3);
  EXPECT_FALSE(IsToastShown(id1));
  EXPECT_FALSE(IsToastShown(id2));
  EXPECT_FALSE(IsToastShown(id3));
  EXPECT_FALSE(GetCurrentOverlay());

  // Confirm that 2 toasts were shown.
  EXPECT_EQ(2, GetToastSerial());
}

TEST_P(ToastManagerImplTest, ReplaceContentsOfQueuedToast) {
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

TEST_P(ToastManagerImplTest, ReplaceContentsOfCurrentToast) {
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

TEST_P(ToastManagerImplTest,
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

TEST_P(ToastManagerImplTest, ToastDismissedOnSessionStateChanges) {
  // Show a toast supported on the lock screen in the unlocked screen.
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration,
                              /*visible_on_lock_screen=*/true);
  EXPECT_TRUE(GetCurrentOverlay());

  // Simulate device lock, toast should be dismissed.
  ChangeLockState(true);
  EXPECT_FALSE(GetCurrentOverlay());

  // Simulate device unlock, overlay should not be visible.
  ChangeLockState(false);
  EXPECT_FALSE(GetCurrentOverlay());

  // Try to show a new toast from within the lock screen, toast should be
  // immediately shown.
  ChangeLockState(true);
  std::string id2 = ShowToast("TEXT1", ToastData::kInfiniteDuration,
                              /*visible_on_lock_screen=*/true);
  EXPECT_TRUE(GetCurrentOverlay());

  // Unlock, toast should be dismissed.
  ChangeLockState(false);
  EXPECT_FALSE(GetCurrentOverlay());
}

TEST_P(ToastManagerImplTest, ToastNotSupportedOnLockScreen) {
  // Show a toast that is not supported on the lock screen.
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration,
                              /*visible_on_lock_screen=*/false);
  EXPECT_TRUE(GetCurrentOverlay());

  // Simulate device lock, overlay should be dismissed.
  ChangeLockState(true);
  EXPECT_FALSE(GetCurrentOverlay());

  // Try to show a new toast from within the lock screen, toast request will be
  // ignored.
  ChangeLockState(true);
  std::string id2 = ShowToast("TEXT1", ToastData::kInfiniteDuration,
                              /*visible_on_lock_screen=*/false);
  EXPECT_FALSE(GetCurrentOverlay());

  // Unlock, overlay should not be visible.
  ChangeLockState(false);
  EXPECT_FALSE(GetCurrentOverlay());
}

TEST_P(ToastManagerImplTest, ShownCountMetric) {
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

TEST_P(ToastManagerImplTest, TimeInQueueMetric) {
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

TEST_P(ToastManagerImplTest, UserJourneyTimeMetric) {
  base::HistogramTester histogram_tester;

  const ToastCatalogName catalog_name = ToastCatalogName::kTestCatalogName;
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
TEST_P(ToastManagerImplTest, ExpiredCallbackRunsWhenToastOverlayClosed) {
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
        toast_id, ToastCatalogName::kTestCatalogName,
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
TEST_P(ToastManagerImplTest, ToastsCanPersistOnHover) {
  std::string toast_id = "TOAST_ID_" + base::NumberToString(GetToastSerial());

  ToastData toast_data(toast_id, ToastCatalogName::kTestCatalogName,
                       /*text=*/u"");
  toast_data.persist_on_hover = true;

  auto* toast_manager = manager();
  toast_manager->Show(std::move(toast_data));
  EXPECT_TRUE(toast_manager->IsToastShown(toast_id));

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
  ASSERT_TRUE(toast_manager->IsToastShown(toast_id));

  // Move the mouse away to resume the expiration countdown timer.
  event_generator->MoveMouseTo(gfx::Point(0, 0));
  ASSERT_FALSE(widget->GetRootView()->IsMouseHovered());

  // Wait for the toast to expire now that the toast is no longer hovered.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);
  EXPECT_FALSE(toast_manager->IsToastShown(toast_id));
}

// Table-driven test that checks that toasts designated to show on all windows
// correctly show and close on all root windows.
TEST_P(ToastManagerImplTest, ShowAndCloseToastsOnAllRootWindows) {
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
        toast_id, ToastCatalogName::kTestCatalogName,
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

    for (aura::Window* root_window : root_windows) {
      EXPECT_TRUE(GetCurrentOverlay(root_window));
    }

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

    for (aura::Window* root_window : root_windows) {
      EXPECT_FALSE(GetCurrentOverlay(root_window));
    }
  }
}

// This tests that toasts that are designated to persist on hover and appear on
// all root windows will not close when one of the toast instances is hovered.
TEST_P(ToastManagerImplTest, ToastsThatPersistOnHoverOnAllRootWindows) {
  UpdateDisplay("800x700,800x700");
  auto* toast_manager = manager();
  const aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  std::string toast_id = "TOAST_ID_" + base::NumberToString(GetToastSerial());

  // Create a basic toast with `ToastData::kDefaultToastDuration` as duration.
  ToastData toast_data(toast_id, ToastCatalogName::kTestCatalogName,
                       /*text=*/u"");

  // Indicate that the toast will show on all root windows and persist on hover.
  toast_data.show_on_all_root_windows = true;
  toast_data.persist_on_hover = true;
  toast_manager->Show(std::move(toast_data));
  ASSERT_TRUE(toast_manager->IsToastShown(toast_id));

  for (aura::Window* root_window : root_windows) {
    ASSERT_TRUE(GetCurrentOverlay(root_window));
  }

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

  for (aura::Window* root_window : root_windows) {
    EXPECT_TRUE(GetCurrentOverlay(root_window));
  }

  // Move the mouse away to resume the expiration countdown timer.
  event_generator->MoveMouseTo(gfx::Point(0, 0));
  ASSERT_FALSE(widget->GetRootView()->IsMouseHovered());

  // Wait for the other half of the toast duration to elapse. This time, because
  // the mouse has been moved away from the toast, all toast instances should be
  // gone.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);

  for (aura::Window* root_window : root_windows) {
    EXPECT_FALSE(GetCurrentOverlay(root_window));
  }
}

// This tests that multi-monitor toast instances do not call the
// `expired_callback_` when the root window is removed.
TEST_P(ToastManagerImplTest, ExpiredCallbackNotCalledOnRootWindowRemoved) {
  UpdateDisplay("800x700,800x700");
  auto* toast_manager = manager();

  std::string toast_id = "TOAST_ID_" + base::NumberToString(GetToastSerial());

  // Create a basic toast with `ToastData::kDefaultToastDuration` as duration.
  ToastData toast_data(toast_id, ToastCatalogName::kTestCatalogName,
                       /*text=*/u"");

  // Indicate that the toast will show on all root windows.
  toast_data.show_on_all_root_windows = true;

  // Bind a lambda that will change a value to tell us whether the expired
  // callback ran.
  bool expired_callback_ran = false;
  toast_data.expired_callback = base::BindLambdaForTesting(
      [&expired_callback_ran]() { expired_callback_ran = true; });
  toast_manager->Show(std::move(toast_data));
  ASSERT_TRUE(toast_manager->IsToastShown(toast_id));

  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    ASSERT_TRUE(GetCurrentOverlay(root_window));
  }

  // Wait for half of the toast duration to elapse.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);

  // Remove a display to trigger the destruction of a toast overlay.
  // `expired_callback_ran` should still be false.
  UpdateDisplay("800x700");
  ASSERT_EQ(1u, Shell::GetAllRootWindows().size());
  ASSERT_TRUE(toast_manager->IsToastShown(toast_id));
  EXPECT_FALSE(expired_callback_ran);

  // Wait for the other half of the toast duration to elapse.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);
  EXPECT_FALSE(toast_manager->IsToastShown(toast_id));
  EXPECT_TRUE(expired_callback_ran);
}

// Tests that toasts are properly closed if they only exist in a secondary
// display that gets removed e.g. by monitor disconnecteded.
TEST_P(ToastManagerImplTest, SingleDisplayToastDestroyedOnRootWindowRemoved) {
  // Add a secondary display, and set it to be the active display so toasts are
  // added here.
  UpdateDisplay("800x700,800x700");
  display::Screen::GetScreen()->SetDisplayForNewWindows(
      GetSecondaryDisplay().id());

  auto* toast_manager = manager();
  std::string toast_id = "TOAST_ID_" + base::NumberToString(GetToastSerial());

  // Create a basic toast with `ToastData::kDefaultToastDuration` as duration.
  ToastData toast_data(toast_id, ToastCatalogName::kTestCatalogName,
                       /*text=*/u"");

  // Indicate that the toast will not show on all root windows.
  toast_data.show_on_all_root_windows = false;

  // Bind a lambda that will change a value to tell us whether the expired
  // callback ran.
  bool expired_callback_ran = false;
  toast_data.expired_callback = base::BindLambdaForTesting(
      [&expired_callback_ran]() { expired_callback_ran = true; });
  toast_manager->Show(std::move(toast_data));
  ASSERT_TRUE(toast_manager->IsToastShown(toast_id));

  // Remove a display to trigger the destruction of a toast overlay. Since this
  // is the only instance of the toast, `expired_callback_ran` should be true.
  UpdateDisplay("800x700");
  ASSERT_EQ(1u, Shell::GetAllRootWindows().size());
  EXPECT_FALSE(toast_manager->IsToastShown(toast_id));
  EXPECT_TRUE(expired_callback_ran);
}

// This tests that new instances of a multi-monitor toast are spawned with the
// correct duration and correct persisting state.
TEST_P(ToastManagerImplTest,
       AllRootWindowToastsCreatedWithCorrectDurationAndPersistState) {
  // Start with display at 800x700 to maintain cursor position when adding root
  // windows.
  UpdateDisplay("800x700");
  auto* toast_manager = manager();

  std::string toast_id = "TOAST_ID_" + base::NumberToString(GetToastSerial());

  // Create a basic toast with `ToastData::kDefaultToastDuration` as duration.
  ToastData toast_data(toast_id, ToastCatalogName::kTestCatalogName,
                       /*text=*/u"");

  // Indicate that the toast will show on all root windows and persist on hover.
  toast_data.show_on_all_root_windows = true;
  toast_data.persist_on_hover = true;
  toast_manager->Show(std::move(toast_data));
  ASSERT_TRUE(toast_manager->IsToastShown(toast_id));

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

  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    EXPECT_TRUE(GetCurrentOverlay(root_window));
  }

  // Unhover the mouse an add a third root window.
  event_generator->MoveMouseTo(gfx::Point(0, 0));
  ASSERT_FALSE(widget->GetRootView()->IsMouseHovered());
  UpdateDisplay("800x700,800x700,800x700");

  // Wait for the remaining half of the toast duration to elapse. At this point
  // all three toast instances should be destroyed.
  WaitForTimeDelta(ToastData::kDefaultToastDuration / 2);
  base::RunLoop().RunUntilIdle();

  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    EXPECT_FALSE(GetCurrentOverlay(root_window));
  }
}

// Tests that an offset is added to shift the overlay baseline up when
// toasts are side aligned and a slider bubble is shown.
// Overlay baseline is unchanged when toasts are not side aligned.
TEST_P(ToastManagerImplTest, BaselineUpdatesAfterSliderBubbleShown) {
  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  const int previous_baseline = GetToastBounds().bottom();

  // The difference between baselines for side aligned toasts after showing a
  // slider bubble should be the slider bubble height + a default spacing
  // offset. Baseline remains unchanged with center aligned toasts.
  GetPrimaryUnifiedSystemTray()->ShowVolumeSliderBubble();
  auto* slider_view = GetPrimaryUnifiedSystemTray()->GetSliderView();
  ASSERT_TRUE(slider_view);
  if (AreSideAlignedToastsEnabled()) {
    EXPECT_EQ(slider_view->height() + ToastOverlay::kOffset,
              previous_baseline - GetToastBounds().bottom());
  } else {
    EXPECT_EQ(GetToastBounds().bottom(), previous_baseline);
  }

  // Baseline returns to previous value when the slider bubble is closed.
  GetPrimaryUnifiedSystemTray()->CloseSecondaryBubbles();
  EXPECT_EQ(GetToastBounds().bottom(), previous_baseline);
}

}  // namespace ash
