// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_manager_impl.h"

#include <string>

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/system/toast_catalog.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/manager/display_manager.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"

namespace ash {

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
  ~DummyEvent() override = default;
};

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

  ToastOverlay* GetCurrentOverlay() {
    return manager_->GetCurrentOverlayForTesting();
  }

  views::Widget* GetCurrentWidget() {
    ToastOverlay* overlay = GetCurrentOverlay();
    return overlay ? overlay->widget_for_testing() : nullptr;
  }

  views::LabelButton* GetDismissButton() {
    ToastOverlay* overlay = GetCurrentOverlay();
    DCHECK(overlay);
    return overlay->dismiss_button_for_testing();
  }

  std::u16string GetCurrentText() {
    ToastOverlay* overlay = GetCurrentOverlay();
    return overlay ? overlay->text_ : std::u16string();
  }

  absl::optional<std::u16string> GetCurrentDismissText() {
    ToastOverlay* overlay = GetCurrentOverlay();
    return overlay ? overlay->dismiss_text_ : std::u16string();
  }

  void ClickDismissButton() {
    views::LabelButton* dismiss_button = GetDismissButton();
    const gfx::Point button_center =
        dismiss_button->GetBoundsInScreen().CenterPoint();
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(button_center);
    event_generator->ClickLeftButton();
  }

  std::string ShowToast(const std::string& text,
                        base::TimeDelta duration,
                        bool visible_on_lock_screen = false) {
    std::string id = "TOAST_ID_" + base::NumberToString(serial_++);
    manager()->Show(ToastData(id, ToastCatalogName::kToastManagerUnittest,
                              base::ASCIIToUTF16(text), duration,
                              visible_on_lock_screen));
    return id;
  }

  std::string ShowToastWithDismiss(
      const std::string& text,
      base::TimeDelta duration,
      const absl::optional<std::string>& dismiss_text) {
    absl::optional<std::u16string> localized_dismiss;
    if (dismiss_text.has_value())
      localized_dismiss = base::ASCIIToUTF16(dismiss_text.value());

    std::string id = "TOAST_ID_" + base::NumberToString(serial_++);
    manager()->Show(ToastData(id, ToastCatalogName::kToastManagerUnittest,
                              base::ASCIIToUTF16(text), duration,
                              /*visible_on_lock_screen=*/false,
                              localized_dismiss));
    return id;
  }

  void CancelToast(const std::string& id) { manager()->Cancel(id); }

  void ReplaceToast(const std::string& id,
                    const std::string& text,
                    base::TimeDelta duration,
                    bool visible_on_lock_screen = false) {
    manager()->Show(ToastData(id, ToastCatalogName::kToastManagerUnittest,
                              base::ASCIIToUTF16(text), duration,
                              visible_on_lock_screen));
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
  ShowToastWithDismiss("DUMMY", ToastData::kInfiniteDuration, "Dismiss");

  EXPECT_EQ(1, GetToastSerial());

  EXPECT_FALSE(GetCurrentWidget()->GetLayer()->GetAnimator()->is_animating());

  ClickDismissButton();

  EXPECT_EQ(nullptr, GetCurrentOverlay());
}

// TODO(crbug.com/959781): Test is flaky.
TEST_F(ToastManagerImplTest, DISABLED_ShowAndCloseManuallyDuringAnimation) {
  ui::ScopedAnimationDurationScaleMode slow_animation_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  ShowToastWithDismiss("DUMMY", ToastData::kInfiniteDuration, "Dismiss");
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
  ShowToastWithDismiss("DUMMY", base::Milliseconds(10),
                       absl::optional<std::string>());
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

  // Wait until the first toast's closing animation has finished.
  task_environment()->FastForwardBy(base::Seconds(1));

  // Confirm that the most recent toast content is visible. The toast serial
  // should be different, indicating the original toast's timeout won't close
  // the new toast's.
  EXPECT_EQ(u"TEXT1_updated2", GetCurrentText());
  EXPECT_EQ(2, GetToastSerial());

  // Cancel the shown toast and wait for the animation to finish.
  CancelToast(id1);
  task_environment()->FastForwardBy(base::Seconds(2));

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

}  // namespace ash
