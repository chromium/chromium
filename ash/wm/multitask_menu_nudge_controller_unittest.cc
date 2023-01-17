// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/multitask_menu_nudge_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/display/display_move_window_util.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/caption_buttons/frame_size_button.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#include "chromeos/ui/frame/multitask_menu/multitask_button.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu.h"
#include "chromeos/ui/wm/features.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class MultitaskMenuNudgeControllerTest : public AshTestBase {
 public:
  MultitaskMenuNudgeControllerTest() = default;
  MultitaskMenuNudgeControllerTest(const MultitaskMenuNudgeControllerTest&) =
      delete;
  MultitaskMenuNudgeControllerTest& operator=(
      const MultitaskMenuNudgeControllerTest&) = delete;
  ~MultitaskMenuNudgeControllerTest() override = default;

  views::Widget* GetWidget() { return controller_->nudge_widget_.get(); }

  void FireDismissNudgeTimer() { controller_->nudge_dismiss_timer_.FireNow(); }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::wm::features::kFloatWindow);

    AshTestBase::SetUp();

    MultitaskMenuNudgeController::SetSuppressNudgeForTesting(false);
    controller_ = Shell::Get()->multitask_menu_nudge_controller();
    controller_->SetOverrideClockForTesting(&test_clock_);

    // Advance the test clock so we aren't at zero time.
    test_clock_.Advance(base::Hours(50));
  }

  void TearDown() override {
    controller_->SetOverrideClockForTesting(nullptr);

    AshTestBase::TearDown();
  }

 protected:
  base::SimpleTestClock test_clock_;

 private:
  MultitaskMenuNudgeController* controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the nudge is shown after resizing a window.
TEST_F(MultitaskMenuNudgeControllerTest, NudgeShownAfterWindowResize) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));

  // Drag to resize from the bottom right corner of `window`.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(300, 300));
  event_generator->PressLeftButton();
  EXPECT_FALSE(GetWidget());

  event_generator->MoveMouseBy(10, 10);
  EXPECT_TRUE(GetWidget());
}

TEST_F(MultitaskMenuNudgeControllerTest, NudgeShownAfterStateChange) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_FALSE(GetWidget());

  WindowState::Get(window.get())->Maximize();
  EXPECT_TRUE(GetWidget());
}

// Tests that there is no crash after toggling fullscreen on and off. Regression
// test for https://crbug.com/1341142.
TEST_F(MultitaskMenuNudgeControllerTest, NoCrashAfterFullscreening) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_FALSE(GetWidget());

  // Turn of animations for immersive mode, so we don't have to wait for the top
  // container to hide on fullscreen.
  auto* immersive_controller = chromeos::ImmersiveFullscreenController::Get(
      views::Widget::GetWidgetForNativeView(window.get()));
  chromeos::ImmersiveFullscreenControllerTestApi(immersive_controller)
      .SetupForTest();

  const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window.get())->OnWMEvent(&event);

  // Window needs to be immersive enabled, but not revealed for the bug to
  // reproduce.
  ASSERT_TRUE(immersive_controller->IsEnabled());
  ASSERT_FALSE(immersive_controller->IsRevealed());

  WindowState::Get(window.get())->OnWMEvent(&event);
  EXPECT_FALSE(GetWidget());
}

// Tests that there is no crash after floating a window via the multitask menu.
// Regression test for b/265189622.
TEST_F(MultitaskMenuNudgeControllerTest,
       NoCrashAfterFloatingFromMultitaskMenu) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_FALSE(GetWidget());

  // Maximize the window to show the nudge.
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  WindowState::Get(window.get())->OnWMEvent(&maximize_event);
  ASSERT_TRUE(GetWidget());

  // Float the window from the multitask menu. Floating the window using the
  // accelerator does not cause the crash mentioned in the bug because the
  // presence of the multitask menu causes an activation change which leads to
  // restacking that does not happen otherwise.
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      std::string("MultitaskMenuBubbleWidget"));
  auto* size_button = static_cast<chromeos::FrameSizeButton*>(
      NonClientFrameViewAsh::Get(window.get())
          ->GetHeaderView()
          ->caption_button_container()
          ->size_button());
  size_button->ShowMultitaskMenu(
      chromeos::MultitaskMenuEntryType::kFrameSizeButtonHover);
  views::WidgetDelegate* delegate =
      waiter.WaitIfNeededAndGet()->widget_delegate();
  auto* multitask_menu =
      static_cast<chromeos::MultitaskMenu*>(delegate->AsDialogDelegate());

  // After floating the window from the multitask menu, there is no crash.
  GetEventGenerator()->MoveMouseTo(
      multitask_menu->multitask_menu_view_for_testing()
          ->float_button_for_testing()
          ->GetBoundsInScreen()
          .CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  EXPECT_TRUE(GetWidget());
}

TEST_F(MultitaskMenuNudgeControllerTest, NudgeTimeout) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  WindowState::Get(window.get())->Maximize();
  ASSERT_TRUE(GetWidget());

  FireDismissNudgeTimer();
  EXPECT_FALSE(GetWidget());
}

// Tests that if a window gets destroyed while the nduge is showing, the nudge
// disappears and there is no crash.
TEST_F(MultitaskMenuNudgeControllerTest, WindowDestroyedWhileNudgeShown) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  WindowState::Get(window.get())->Maximize();
  ASSERT_TRUE(GetWidget());

  window.reset();
  EXPECT_FALSE(GetWidget());
}

TEST_F(MultitaskMenuNudgeControllerTest, NudgeMultiDisplay) {
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto window = CreateAppWindow(gfx::Rect(300, 300));

  // Maximize and restore so the nudge shows and we can still drag the window.
  WindowState::Get(window.get())->Maximize();
  WindowState::Get(window.get())->Restore();
  ASSERT_TRUE(GetWidget());

  // Drag from the caption the window to the other display. The nudge should be
  // on the other display, even though the window is not (the window stays
  // offscreen and a mirrored version called the drag window is the one on the
  // secondary display).
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(150, 10));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(900, 0));
  EXPECT_EQ(Shell::GetAllRootWindows()[1],
            GetWidget()->GetNativeWindow()->GetRootWindow());

  event_generator->ReleaseLeftButton();
  EXPECT_EQ(Shell::GetAllRootWindows()[1],
            GetWidget()->GetNativeWindow()->GetRootWindow());

  display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
  EXPECT_EQ(Shell::GetAllRootWindows()[0],
            GetWidget()->GetNativeWindow()->GetRootWindow());
}

// Tests that based on preferences (shown count, and last shown time), the nudge
// may or may not be shown.
TEST_F(MultitaskMenuNudgeControllerTest, NudgePreferences) {
  // Maximize the window to show the nudge for the first time.
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  WindowState::Get(window.get())->Maximize();
  ASSERT_TRUE(GetWidget());
  FireDismissNudgeTimer();
  ASSERT_FALSE(GetWidget());

  // Restore the window. This does not show the nudge as 24 hours have not
  // elapsed since the nudge was shown.
  WindowState::Get(window.get())->Restore();
  ASSERT_FALSE(GetWidget());

  // Maximize and try restoring again after waiting 25 hours. The nudge should
  // now show for the second time.
  WindowState::Get(window.get())->Maximize();
  test_clock_.Advance(base::Hours(25));
  WindowState::Get(window.get())->Restore();
  ASSERT_TRUE(GetWidget());
  FireDismissNudgeTimer();
  ASSERT_FALSE(GetWidget());

  // Show the nudge for a third time. This will be the last time it is shown.
  test_clock_.Advance(base::Hours(25));
  WindowState::Get(window.get())->Maximize();
  ASSERT_TRUE(GetWidget());
  FireDismissNudgeTimer();
  ASSERT_FALSE(GetWidget());

  // Advance the clock and attempt to show the nudge for a forth time. Verify
  // that it will not show.
  test_clock_.Advance(base::Hours(25));
  WindowState::Get(window.get())->Restore();
  EXPECT_FALSE(GetWidget());
}

}  // namespace ash
