// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_manager.h"

#include "ash/screen_util.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/manager/display_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
  ~DummyEvent() override = default;
};

class ToastManagerTest : public AshTestBase {
 public:
  ToastManagerTest() = default;
  ~ToastManagerTest() override = default;

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
  ToastManager* manager() { return manager_; }

  int GetToastSerial() { return manager_->serial_for_testing(); }

  ToastOverlay* GetCurrentOverlay() {
    return manager_->GetCurrentOverlayForTesting();
  }

  views::Widget* GetCurrentWidget() {
    ToastOverlay* overlay = GetCurrentOverlay();
    return overlay ? overlay->widget_for_testing() : nullptr;
  }

  ToastOverlayButton* GetDismissButton() {
    ToastOverlay* overlay = GetCurrentOverlay();
    DCHECK(overlay);
    return overlay->dismiss_button_for_testing();
  }

  base::string16 GetCurrentText() {
    ToastOverlay* overlay = GetCurrentOverlay();
    return overlay ? overlay->text_ : base::string16();
  }

  base::Optional<base::string16> GetCurrentDismissText() {
    ToastOverlay* overlay = GetCurrentOverlay();
    return overlay ? overlay->dismiss_text_ : base::string16();
  }

  void ClickDismissButton() {
    ToastOverlay* overlay = GetCurrentOverlay();
    if (overlay)
      overlay->ClickDismissButtonForTesting(DummyEvent());
  }

  std::string ShowToast(const std::string& text,
                        int32_t duration,
                        bool visible_on_lock_screen = false) {
    std::string id = "TOAST_ID_" + base::UintToString(serial_++);
    manager()->Show(ToastData(id, base::ASCIIToUTF16(text), duration,
                              base::string16(), visible_on_lock_screen));
    return id;
  }

  std::string ShowToastWithDismiss(
      const std::string& text,
      int32_t duration,
      const base::Optional<std::string>& dismiss_text) {
    base::Optional<base::string16> localized_dismiss;
    if (dismiss_text.has_value())
      localized_dismiss = base::ASCIIToUTF16(dismiss_text.value());

    std::string id = "TOAST_ID_" + base::UintToString(serial_++);
    manager()->Show(
        ToastData(id, base::ASCIIToUTF16(text), duration, localized_dismiss));
    return id;
  }

  void CancelToast(const std::string& id) { manager()->Cancel(id); }

  void ChangeLockState(bool lock) {
    mojom::SessionInfoPtr info_ptr = mojom::SessionInfo::New();
    info_ptr->state = lock ? session_manager::SessionState::LOCKED
                           : session_manager::SessionState::ACTIVE;
    Shell::Get()->session_controller()->SetSessionInfo(std::move(info_ptr));
  }

 private:
  ToastManager* manager_ = nullptr;
  unsigned int serial_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ToastManagerTest);
};

TEST_F(ToastManagerTest, ShowAndCloseAutomatically) {
  ShowToast("DUMMY", 10);

  EXPECT_EQ(1, GetToastSerial());

  while (GetCurrentOverlay() != nullptr)
    base::RunLoop().RunUntilIdle();
}

TEST_F(ToastManagerTest, ShowAndCloseManually) {
  ShowToast("DUMMY", ToastData::kInfiniteDuration);

  EXPECT_EQ(1, GetToastSerial());

  EXPECT_FALSE(GetCurrentWidget()->GetLayer()->GetAnimator()->is_animating());

  ClickDismissButton();

  EXPECT_EQ(nullptr, GetCurrentOverlay());
}

TEST_F(ToastManagerTest, ShowAndCloseManuallyDuringAnimation) {
  ui::ScopedAnimationDurationScaleMode slow_animation_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
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

TEST_F(ToastManagerTest, NullMessageHasNoDismissButton) {
  ShowToastWithDismiss("DUMMY", 10, base::Optional<std::string>());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetDismissButton());
}

TEST_F(ToastManagerTest, QueueMessage) {
  ShowToast("DUMMY1", 10);
  ShowToast("DUMMY2", 10);
  ShowToast("DUMMY3", 10);

  EXPECT_EQ(1, GetToastSerial());
  EXPECT_EQ(base::ASCIIToUTF16("DUMMY1"), GetCurrentText());

  while (GetToastSerial() != 2)
    base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::ASCIIToUTF16("DUMMY2"), GetCurrentText());

  while (GetToastSerial() != 3)
    base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::ASCIIToUTF16("DUMMY3"), GetCurrentText());
}

TEST_F(ToastManagerTest, PositionWithVisibleBottomShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(shelf->GetUserWorkAreaBounds()));
  EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(), 1);

  gfx::Rect shelf_bounds = shelf->GetIdealBounds();
  EXPECT_FALSE(toast_bounds.Intersects(shelf_bounds));
  EXPECT_EQ(shelf_bounds.y() - ToastOverlay::kOffset, toast_bounds.bottom());
  EXPECT_EQ(
      root_bounds.bottom() - shelf_bounds.height() - ToastOverlay::kOffset,
      toast_bounds.bottom());
}

TEST_F(ToastManagerTest, PositionWithAutoHiddenBottomShelf) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));

  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(shelf->GetUserWorkAreaBounds()));
  EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(), 1);
  EXPECT_EQ(root_bounds.bottom() - kHiddenShelfInScreenPortion -
                ToastOverlay::kOffset,
            toast_bounds.bottom());
}

TEST_F(ToastManagerTest, PositionWithHiddenBottomShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(shelf->GetUserWorkAreaBounds()));
  EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(), 1);
  EXPECT_EQ(root_bounds.bottom() - ToastOverlay::kOffset,
            toast_bounds.bottom());
}

TEST_F(ToastManagerTest, PositionWithVisibleLeftShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::RectF precise_toast_bounds(toast_bounds);
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(shelf->GetUserWorkAreaBounds()));
  EXPECT_EQ(root_bounds.bottom() - ToastOverlay::kOffset,
            toast_bounds.bottom());

  gfx::Rect shelf_bounds = shelf->GetIdealBounds();
  EXPECT_FALSE(toast_bounds.Intersects(shelf_bounds));
  EXPECT_NEAR(
      shelf_bounds.right() + (root_bounds.width() - shelf_bounds.width()) / 2.0,
      precise_toast_bounds.CenterPoint().x(), 1.f /* accepted error */);
}

TEST_F(ToastManagerTest, PositionWithUnifiedDesktop) {
  display_manager()->SetUnifiedDesktopEnabled(true);
  UpdateDisplay("1000x500,0+600-100x500");

  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ShowToast("DUMMY", ToastData::kInfiniteDuration);
  EXPECT_EQ(1, GetToastSerial());

  gfx::Rect toast_bounds = GetCurrentWidget()->GetWindowBoundsInScreen();
  gfx::Rect root_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf->GetWindow());

  EXPECT_TRUE(toast_bounds.Intersects(shelf->GetUserWorkAreaBounds()));
  EXPECT_TRUE(root_bounds.Contains(toast_bounds));
  EXPECT_NEAR(root_bounds.CenterPoint().x(), toast_bounds.CenterPoint().x(), 1);

  gfx::Rect shelf_bounds = shelf->GetIdealBounds();
  EXPECT_FALSE(toast_bounds.Intersects(shelf_bounds));
  EXPECT_EQ(shelf_bounds.y() - ToastOverlay::kOffset, toast_bounds.bottom());
  EXPECT_EQ(
      root_bounds.bottom() - shelf_bounds.height() - ToastOverlay::kOffset,
      toast_bounds.bottom());
}

TEST_F(ToastManagerTest, CancelToast) {
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration);
  std::string id2 = ShowToast("TEXT2", ToastData::kInfiniteDuration);
  std::string id3 = ShowToast("TEXT3", ToastData::kInfiniteDuration);

  // Confirm that the first toast is shown.
  EXPECT_EQ(base::ASCIIToUTF16("TEXT1"), GetCurrentText());
  // Cancel the queued toast.
  CancelToast(id2);
  // Confirm that the shown toast is still visible.
  EXPECT_EQ(base::ASCIIToUTF16("TEXT1"), GetCurrentText());
  // Cancel the shown toast.
  CancelToast(id1);
  // Confirm that the next toast is visible.
  EXPECT_EQ(base::ASCIIToUTF16("TEXT3"), GetCurrentText());
  // Cancel the shown toast.
  CancelToast(id3);
  // Confirm that the shown toast disappears.
  EXPECT_FALSE(GetCurrentOverlay());
  // Confirm that only 1 toast is shown.
  EXPECT_EQ(2, GetToastSerial());
}

TEST_F(ToastManagerTest, ShowToastOnLockScreen) {
  // Simulate device lock.
  ChangeLockState(true);

  // Trying to show a toast.
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration);
  // Confirm that it's not visible because it's queued.
  EXPECT_EQ(nullptr, GetCurrentOverlay());

  // Simulate device unlock.
  ChangeLockState(false);
  EXPECT_TRUE(GetCurrentOverlay());
  EXPECT_EQ(base::ASCIIToUTF16("TEXT1"), GetCurrentText());
}

TEST_F(ToastManagerTest, ShowSupportedToastOnLockScreen) {
  // Simulate device lock.
  ChangeLockState(true);

  // Trying to show a toast.
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration,
                              /*visible_on_lock_screen=*/true);
  // Confirm it's visible and not queued.
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(base::ASCIIToUTF16("TEXT1"), GetCurrentText());

  // Simulate device unlock.
  ChangeLockState(false);
  // Confirm that the toast is still visible.
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(base::ASCIIToUTF16("TEXT1"), GetCurrentText());
}

TEST_F(ToastManagerTest, DeferToastByLockScreen) {
  // Show a toast.
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration,
                              /*visible_on_lock_screen=*/true);
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(base::ASCIIToUTF16("TEXT1"), GetCurrentText());

  // Simulate device lock.
  ChangeLockState(true);
  // Confirm that it gets hidden.
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(base::ASCIIToUTF16("TEXT1"), GetCurrentText());

  // Simulate device unlock.
  ChangeLockState(false);
  // Confirm that it gets visible again.
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(base::ASCIIToUTF16("TEXT1"), GetCurrentText());
}

TEST_F(ToastManagerTest, NotDeferToastForLockScreen) {
  // Show a toast.
  std::string id1 = ShowToast("TEXT1", ToastData::kInfiniteDuration,
                              /*visible_on_lock_screen=*/false);
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(base::ASCIIToUTF16("TEXT1"), GetCurrentText());

  // Simulate device lock.
  ChangeLockState(true);
  // Confirm that it gets hidden.
  EXPECT_EQ(nullptr, GetCurrentOverlay());

  // Simulate device unlock.
  ChangeLockState(false);
  // Confirm that it gets visible again.
  EXPECT_NE(nullptr, GetCurrentOverlay());
  EXPECT_EQ(base::ASCIIToUTF16("TEXT1"), GetCurrentText());
}

}  // namespace ash
