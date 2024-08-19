// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/display/display_move_window_util.h"
#include "ash/frame/multitask_menu_nudge_delegate_ash.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_cue_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/ui/base/nudge_util.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#include "chromeos/ui/frame/multitask_menu/multitask_button.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view_test_api.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Returns the nudge controller associated with `window`.
chromeos::MultitaskMenuNudgeController* GetNudgeControllerForWindow(
    aura::Window* window) {
  if (display::Screen::GetScreen()->InTabletMode()) {
    return TabletModeControllerTestApi()
        .tablet_mode_window_manager()
        ->tablet_mode_multitask_menu_controller()
        ->multitask_cue_controller()
        ->nudge_controller_for_testing();
  }

  if (auto* frame = NonClientFrameViewAsh::Get(window)) {
    return chromeos::FrameCaptionButtonContainerView::TestApi(
               frame->GetHeaderView()->caption_button_container())
        .nudge_controller();
  }

  return nullptr;
}

}  // namespace

class MultitaskMenuNudgeControllerTest : public AshTestBase {
 public:
  MultitaskMenuNudgeControllerTest() = default;
  MultitaskMenuNudgeControllerTest(const MultitaskMenuNudgeControllerTest&) =
      delete;
  MultitaskMenuNudgeControllerTest& operator=(
      const MultitaskMenuNudgeControllerTest&) = delete;
  ~MultitaskMenuNudgeControllerTest() override = default;

  views::Widget* GetNudgeWidgetForWindow(aura::Window* window) {
    chromeos::MultitaskMenuNudgeController* controller =
        GetNudgeControllerForWindow(window);
    return controller ? controller->nudge_widget_.get() : nullptr;
  }

  void FireDismissNudgeTimer(aura::Window* window) {
    if (chromeos::MultitaskMenuNudgeController* controller =
            GetNudgeControllerForWindow(window)) {
      controller->clamshell_nudge_dismiss_timer_.FireNow();
    }
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    chromeos::MultitaskMenuNudgeController::SetSuppressNudgeForTesting(false);
    chromeos::MultitaskMenuNudgeController::SetOverrideClockForTesting(
        &test_clock_);

    // Advance the test clock so we aren't at zero time.
    test_clock_.Advance(base::Hours(50));
  }

  void TearDown() override {
    chromeos::MultitaskMenuNudgeController::SetOverrideClockForTesting(nullptr);

    AshTestBase::TearDown();
  }

 protected:
  // Tests that the tablet mode nudge bounds in screen are correct.
  void ExpectCorrectTabletNudgeBounds(aura::Window* window) {
    const gfx::Size size =
        GetNudgeWidgetForWindow(window)->GetContentsView()->GetPreferredSize();
    const auto window_screen_bounds = window->GetBoundsInScreen();
    const int tablet_nudge_y_offset =
        MultitaskMenuNudgeDelegateAsh::kTabletNudgeAdditionalYOffset +
        TabletModeMultitaskCueController::kCueHeight +
        TabletModeMultitaskCueController::kCueYOffset;
    const gfx::Rect expected_bounds(
        (window_screen_bounds.width() - size.width()) / 2 +
            window_screen_bounds.x(),
        tablet_nudge_y_offset + window_screen_bounds.y(), size.width(),
        size.height());
    EXPECT_EQ(expected_bounds,
              GetNudgeWidgetForWindow(window)->GetWindowBoundsInScreen());
  }

  base::SimpleTestClock test_clock_;
};

// Tests that there is no crash after toggling fullscreen on and off. Regression
// test for https://crbug.com/1341142.
TEST_F(MultitaskMenuNudgeControllerTest, NoCrashAfterFullscreening) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));

  // Turn of animations for immersive mode, so we don't have to wait for the top
  // container to hide on fullscreen.
  auto* immersive_controller = chromeos::ImmersiveFullscreenController::Get(
      views::Widget::GetWidgetForNativeView(window.get()));
  chromeos::ImmersiveFullscreenControllerTestApi(immersive_controller)
      .SetupForTest();

  const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window.get())->OnWMEvent(&event);
  EXPECT_FALSE(GetNudgeWidgetForWindow(window.get()));

  // Window needs to be immersive enabled, but not revealed for the bug to
  // reproduce.
  ASSERT_TRUE(immersive_controller->IsEnabled());
  ASSERT_FALSE(immersive_controller->IsRevealed());

  WindowState::Get(window.get())->OnWMEvent(&event);
  EXPECT_FALSE(GetNudgeWidgetForWindow(window.get()));
}

// Tests that there is no crash after floating a window via the multitask menu.
// Regression test for http://b/265189622.
TEST_F(MultitaskMenuNudgeControllerTest,
       NoCrashAfterFloatingFromMultitaskMenu) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));

  // Float the window from the multitask menu. Floating the window using the
  // accelerator does not cause the crash mentioned in the bug because the
  // presence of the multitask menu causes an activation change which leads to
  // restacking that does not happen otherwise.
  chromeos::MultitaskMenu* multitask_menu =
      ShowAndWaitMultitaskMenuForWindow(window.get());

  // After floating the window from the multitask menu, there is no crash.
  LeftClickOn(
      chromeos::MultitaskMenuViewTestApi(multitask_menu->multitask_menu_view())
          .GetFloatButton());
  EXPECT_TRUE(WindowState::Get(window.get())->IsFloated());
}

// Tests that there is no crash after entering tablet mode with the multitask
// menu created on the secondary display. Regression test for
// http://b/278165707.
TEST_F(MultitaskMenuNudgeControllerTest,
       NoCrashAfterEnterTabletFromMultidisplay) {
  UpdateDisplay("800x600,801+0-800x600");

  auto window = CreateAppWindow(gfx::Rect(900, 0, 300, 300));
  ASSERT_EQ(Shell::GetAllRootWindows()[1], window->GetRootWindow());

  // Ensure that the clamshell nudge is closed and advance the clock so that the
  // tablet one will show.
  FireDismissNudgeTimer(window.get());
  test_clock_.Advance(base::Hours(26));

  // We use non zero duration since we want to mimic real behavior of stacking
  // order changed on `window` before tablet mode is entered.
  ui::ScopedAnimationDurationScaleMode scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  TabletModeControllerTestApi().EnterTabletMode();
}

// Tests that there is no crash after a window is placed such that the nudge
// widget should be offscreen. Regression test for http://b/282994793.
TEST_F(MultitaskMenuNudgeControllerTest,
       NoCrashAfterActivatingMostlyOffscreenWindowMultidisplay) {
  // Crash is multidisplay related since it involves switching root windows.
  UpdateDisplay("1600x1000,1601+0-1200x1000");

  // Create two windows so we can reactivate `window2` to simulate the crash
  // because the window manager will shift `window2` onscreen if we try to
  // create it offscreen directly.
  auto window1 = CreateAppWindow(gfx::Rect(300, 300));
  auto window2 = CreateAppWindow(gfx::Rect(1000, 300));

  // Place `window2` mostly offscreen on primary display, such that on
  // activation, the nudge widget should not be seen.
  window2->SetBounds(gfx::Rect(1400, 0, 1000, 300));
  ASSERT_EQ(Shell::GetAllRootWindows()[0], window2->GetRootWindow());

  // The nudge widget was shown on `window1` since it was created first. Dismiss
  // it and advance the clock so it will show on the next window activation.
  ASSERT_TRUE(GetNudgeWidgetForWindow(window1.get()));
  FireDismissNudgeTimer(window1.get());
  wm::ActivateWindow(window1.get());
  test_clock_.Advance(base::Hours(26));

  // Activate `window2`. Verify that the nudge widget is not created since the
  // anchor is invisible.
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(GetNudgeWidgetForWindow(window2.get()));
}

TEST_F(MultitaskMenuNudgeControllerTest, NudgeTimeout) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));

  FireDismissNudgeTimer(window.get());
  EXPECT_FALSE(GetNudgeWidgetForWindow(window.get()));
}

TEST_F(MultitaskMenuNudgeControllerTest, NoNudgeForNewUser) {
  chromeos::MultitaskMenuNudgeController::SetSuppressNudgeForTesting(false);

  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager{std::make_unique<user_manager::FakeUserManager>()};
  fake_user_manager->SetIsCurrentUserNew(true);

  auto window = CreateAppWindow(gfx::Rect(300, 300));
  EXPECT_FALSE(GetNudgeWidgetForWindow(window.get()));
}

TEST_F(MultitaskMenuNudgeControllerTest, Metrics) {
  base::HistogramTester histogram_tester;

  // Create and activate a window. Test the histogram is recorded.
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   chromeos::kNotifierFrameworkNudgeShownCountHistogram,
                   NudgeCatalogName::kMultitaskMenuClamshell));

  // Simulate opening the multitask menu within 1 minute. Test that the
  // "Within1m" histogram is recorded.
  test_clock_.Advance(base::Seconds(50));
  GetNudgeControllerForWindow(window.get())
      ->OnMenuOpened(/*tablet_mode=*/false);

  const std::string kHistogramPrefix =
      "Ash.NotifierFramework.Nudge.TimeToAction.";
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[kHistogramPrefix + "Within1m"] = 1;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(kHistogramPrefix),
              testing::ContainerEq(expected_counts));

  // Once the user opens the multitask menu, the nudge is no longer shown.
  // Forcefully reset it so we can proceed to the next test. Also advance the
  // clock as the nudge only shows after 24 hours have elapsed.
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kMultitaskMenuNudgeClamshellShownCount, 0);
  test_clock_.Advance(base::Days(2));

  // Destroy the window and recreate and activate it.
  window.reset();
  window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));
  EXPECT_EQ(2, histogram_tester.GetBucketCount(
                   chromeos::kNotifierFrameworkNudgeShownCountHistogram,
                   NudgeCatalogName::kMultitaskMenuClamshell));

  // Simulate opening the multitask menu within 1 hour. Test that the "Within1h"
  // histogram is recorded.
  test_clock_.Advance(base::Minutes(50));
  GetNudgeControllerForWindow(window.get())
      ->OnMenuOpened(/*tablet_mode=*/false);
  expected_counts[kHistogramPrefix + "Within1h"] = 1;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(kHistogramPrefix),
              testing::ContainerEq(expected_counts));

  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kMultitaskMenuNudgeClamshellShownCount, 0);
  test_clock_.Advance(base::Days(2));

  window.reset();
  window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));
  EXPECT_EQ(3, histogram_tester.GetBucketCount(
                   chromeos::kNotifierFrameworkNudgeShownCountHistogram,
                   NudgeCatalogName::kMultitaskMenuClamshell));

  // Simulate opening the multitask menu after a long time. Test that the
  // "WithinSession" histogram is recorded.
  test_clock_.Advance(base::Hours(50));
  GetNudgeControllerForWindow(window.get())
      ->OnMenuOpened(/*tablet_mode=*/false);
  expected_counts[kHistogramPrefix + "WithinSession"] = 1;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(kHistogramPrefix),
              testing::ContainerEq(expected_counts));
}

// Tests that the nudge bounds is within display bounds when the associated
// window is maximized.
TEST_F(MultitaskMenuNudgeControllerTest, ClamshellNudgeBounds) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));

  WindowState::Get(window.get())->Maximize();
  auto* nudge_widget = GetNudgeWidgetForWindow(window.get());
  ASSERT_TRUE(nudge_widget);
  EXPECT_TRUE(display::Screen::GetScreen()
                  ->GetDisplayNearestView(window.get())
                  .work_area()
                  .Contains(nudge_widget->GetWindowBoundsInScreen()));

  // Cleanup some state for the next test.
  FireDismissNudgeTimer(window.get());
  window.reset();
  test_clock_.Advance(base::Hours(26));

  // Test the same thing in RTL.
  base::i18n::SetRTLForTesting(true);
  window = CreateAppWindow(gfx::Rect(300, 300));
  WindowState::Get(window.get())->Maximize();
  nudge_widget = GetNudgeWidgetForWindow(window.get());
  ASSERT_TRUE(nudge_widget);
  EXPECT_TRUE(display::Screen::GetScreen()
                  ->GetDisplayNearestView(window.get())
                  .work_area()
                  .Contains(nudge_widget->GetWindowBoundsInScreen()));
}

TEST_F(MultitaskMenuNudgeControllerTest, NudgeMultiDisplay) {
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));

  // Move the window using the shortcut. Test that the nudge is on the correct
  // display.
  display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
  EXPECT_EQ(Shell::GetAllRootWindows()[1], GetNudgeWidgetForWindow(window.get())
                                               ->GetNativeWindow()
                                               ->GetRootWindow());

  // Drag from the caption the window to the other display. The nudge should be
  // gone, but there is no crash.
  display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(150, 10));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(1200, 0));
  EXPECT_FALSE(GetNudgeWidgetForWindow(window.get()));
}

// Tests that based on preferences (shown count, and last shown time), the nudge
// may or may not be shown.
TEST_F(MultitaskMenuNudgeControllerTest, NudgePreferences) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));
  FireDismissNudgeTimer(window.get());
  ASSERT_FALSE(GetNudgeWidgetForWindow(window.get()));

  // Create the window. This does not show the nudge as 24 hours have not
  // elapsed since the nudge was shown.
  window.reset();
  window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_FALSE(GetNudgeWidgetForWindow(window.get()));

  // Create the window again after waiting 25 hours. The nudge should now show
  // for the second time.
  test_clock_.Advance(base::Hours(25));
  window.reset();
  window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));
  FireDismissNudgeTimer(window.get());
  ASSERT_FALSE(GetNudgeWidgetForWindow(window.get()));

  // Show the nudge for a third time. This will be the last time it is shown.
  test_clock_.Advance(base::Hours(25));
  window.reset();
  window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));
  FireDismissNudgeTimer(window.get());
  ASSERT_FALSE(GetNudgeWidgetForWindow(window.get()));

  // Advance the clock and attempt to show the nudge for a fourth time. Verify
  // that it will not show.
  test_clock_.Advance(base::Hours(25));
  window.reset();
  window = CreateAppWindow(gfx::Rect(300, 300));
  EXPECT_FALSE(GetNudgeWidgetForWindow(window.get()));
}

// Tests that after the multitask menu is shown, the nudge does not show
// anymore.
TEST_F(MultitaskMenuNudgeControllerTest, MenuShown) {
  // Create a window, the nudge is shown on new window activation.
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));

  // When opening the multitask menu, the nudge should dismiss immediately.
  std::ignore = ShowAndWaitMultitaskMenuForWindow(window.get());
  EXPECT_FALSE(GetNudgeWidgetForWindow(window.get()));

  // Advance the clock and then destroy the window and create a new window.
  // Test that the nudge does not show up.
  test_clock_.Advance(base::Hours(25));
  window.reset();
  window = CreateAppWindow(gfx::Rect(300, 300));
  EXPECT_FALSE(GetNudgeWidgetForWindow(window.get()));
}

// Tests that the nudge gets properly hidden after switching desks with a
// floated window. Regression test for http://b/276786909.
TEST_F(MultitaskMenuNudgeControllerTest, FloatedWindowNudge) {
  // Create a new desk.
  NewDesk();
  ASSERT_TRUE(DesksController::Get()->desks()[0]->is_active());

  // Create a floated window, the nudge is shown on new window activation.
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(window.get())->IsFloated());
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));

  ActivateDesk(DesksController::Get()->desks()[1].get());
  EXPECT_FALSE(GetNudgeWidgetForWindow(window.get()));
}

// Tests that the nudge works in tablet mode, and that its bounds in screen are
// correct.
TEST_F(MultitaskMenuNudgeControllerTest, TabletNudgeBounds) {
  TabletModeControllerTestApi().EnterTabletMode();

  // The widget should appear the first time a window is activated.
  auto window = CreateAppWindow();
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));

  // Test that the widget is shown at the correct bounds when the window is
  // first created.
  ExpectCorrectTabletNudgeBounds(window.get());

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());

  // Tests that the widget is shown at the correct bounds when the window is
  // snapped in the primary position.
  split_view_controller->SnapWindow(window.get(), SnapPosition::kPrimary);
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));
  ExpectCorrectTabletNudgeBounds(window.get());

  // Tests that the widget is shown at the correct bounds when the window is
  // snapped in the secondary position.
  split_view_controller->SnapWindow(window.get(), SnapPosition::kSecondary);
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));
  ExpectCorrectTabletNudgeBounds(window.get());
}

// Tests that if a window gets destroyed while the nduge is showing in tablet
// mode, the nudge disappears and there is no crash.
TEST_F(MultitaskMenuNudgeControllerTest, TabletWindowDestroyedWhileNudgeShown) {
  TabletModeControllerTestApi().EnterTabletMode();

  auto window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_TRUE(GetNudgeWidgetForWindow(window.get()));

  window.reset();
  EXPECT_FALSE(GetNudgeWidgetForWindow(window.get()));
}

}  // namespace ash
