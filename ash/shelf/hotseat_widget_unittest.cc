// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/hotseat_widget.h"

#include <memory>
#include <tuple>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/test/assistant_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shelf/drag_window_from_shelf_controller_test_api.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_metrics.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/test/hotseat_state_watcher.h"
#include "ash/shelf/test/shelf_layout_manager_test_base.h"
#include "ash/shelf/test/widget_animation_smoothness_inspector.h"
#include "ash/shell.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/layer_animation_verifier.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/widget_animation_waiter.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {
ShelfWidget* GetShelfWidget() {
  return AshTestBase::GetPrimaryShelf()->shelf_widget();
}

ShelfLayoutManager* GetShelfLayoutManager() {
  return AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
}
}  // namespace

class HotseatWidgetTest
    : public ShelfLayoutManagerTestBase,
      public testing::WithParamInterface<
          std::tuple<ShelfAutoHideBehavior,
                     /*is_assistant_enabled*/ bool,
                     /*navigation_buttons_shown_in_tablet_mode*/ bool>> {
 public:
  HotseatWidgetTest()
      : ShelfLayoutManagerTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        shelf_auto_hide_behavior_(std::get<0>(GetParam())),
        is_assistant_enabled_(std::get<1>(GetParam())),
        navigation_buttons_shown_in_tablet_mode_(std::get<2>(GetParam())) {
    if (is_assistant_enabled_)
      assistant_test_api_ = AssistantTestApi::Create();
  }

  // testing::Test:
  void SetUp() override {
    SetupFeatureLists();
    ShelfLayoutManagerTestBase::SetUp();

    if (is_assistant_enabled_) {
      assistant_test_api_->SetAssistantEnabled(true);
      assistant_test_api_->GetAssistantState()->NotifyFeatureAllowed(
          assistant::AssistantAllowedState::ALLOWED);
      assistant_test_api_->GetAssistantState()->NotifyStatusChanged(
          assistant::AssistantStatus::READY);

      assistant_test_api_->WaitUntilIdle();
    }
  }

  virtual void SetupFeatureLists() {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kHideShelfControlsInTabletMode,
          !navigation_buttons_shown_in_tablet_mode()}});
  }

  void TearDown() override {
    // Some tests may override this value, make sure it's reset.
    ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        false);
    ShelfLayoutManagerTestBase::TearDown();
  }

  ShelfAutoHideBehavior shelf_auto_hide_behavior() const {
    return shelf_auto_hide_behavior_;
  }
  bool is_assistant_enabled() const { return is_assistant_enabled_; }
  bool navigation_buttons_shown_in_tablet_mode() const {
    return navigation_buttons_shown_in_tablet_mode_;
  }
  AssistantTestApi* assistant_test_api() { return assistant_test_api_.get(); }

  void ShowShelfAndActivateAssistant() {
    if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways)
      SwipeUpOnShelf();

    // If the launcher button is not expected to be shown, show the assistant UI
    // directly; otherwise, simulate the long press on the home button,
    if (!navigation_buttons_shown_in_tablet_mode_ &&
        display::Screen::GetScreen()->InTabletMode()) {
      AssistantUiController::Get()->ShowUi(
          assistant::AssistantEntryPoint::kLongPressLauncher);
      return;
    }

    views::View* home_button =
        GetPrimaryShelf()->navigation_widget()->GetHomeButton();
    auto center_point = home_button->GetBoundsInScreen().CenterPoint();

    GetEventGenerator()->set_current_screen_location(center_point);
    GetEventGenerator()->PressTouch();
    GetAppListTestHelper()->WaitUntilIdle();

    // Advance clock to make sure long press gesture is triggered.
    task_environment()->AdvanceClock(base::Seconds(5));
    GetAppListTestHelper()->WaitUntilIdle();

    GetEventGenerator()->ReleaseTouch();
    GetAppListTestHelper()->WaitUntilIdle();
  }

  void ShowShelfAndGoHome() {
    // If the launcher button is not expected to be shown, go home directly;
    // otherwise, simulate tap on the home button,
    if (!navigation_buttons_shown_in_tablet_mode_ &&
        display::Screen::GetScreen()->InTabletMode()) {
      Shell::Get()->app_list_controller()->GoHome(GetPrimaryDisplay().id());
      return;
    }

    // Ensure the shelf, and the home button, are visible.
    if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways)
      SwipeUpOnShelf();
    views::View* home_button =
        GetPrimaryShelf()->navigation_widget()->GetHomeButton();
    GetEventGenerator()->GestureTapAt(
        home_button->GetBoundsInScreen().CenterPoint());
  }

  void StartOverview() {
    ASSERT_FALSE(OverviewController::Get()->InOverviewSession());

    // If the overview button is not expected to be shown, start overview
    // directly; otherwise, simulate tap on the overview button, which should
    // toggle overview.
    if (!navigation_buttons_shown_in_tablet_mode_ &&
        display::Screen::GetScreen()->InTabletMode()) {
      EnterOverview();
      return;
    }

    const gfx::Point overview_button_center = GetPrimaryShelf()
                                                  ->status_area_widget()
                                                  ->overview_button_tray()
                                                  ->GetBoundsInScreen()
                                                  .CenterPoint();
    GetEventGenerator()->GestureTapAt(overview_button_center);
  }

  void EndOverview() {
    ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

    // If the overview button is not expected to be shown, end overview
    // directly; otherwise, simulate tap on the overview button, which should
    // toggle overview.
    if (!navigation_buttons_shown_in_tablet_mode_ &&
        display::Screen::GetScreen()->InTabletMode()) {
      ExitOverview();
      return;
    }

    const gfx::Point overview_button_center = GetPrimaryShelf()
                                                  ->status_area_widget()
                                                  ->overview_button_tray()
                                                  ->GetBoundsInScreen()
                                                  .CenterPoint();
    GetEventGenerator()->GestureTapAt(overview_button_center);
  }

 protected:
  const ShelfAutoHideBehavior shelf_auto_hide_behavior_;
  const bool is_assistant_enabled_;
  const bool navigation_buttons_shown_in_tablet_mode_;
  std::unique_ptr<AssistantTestApi> assistant_test_api_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class HotseatWidgetForestTest : public HotseatWidgetTest {
 public:
  HotseatWidgetForestTest() = default;
  ~HotseatWidgetForestTest() = default;

  // HotseatWidgetTest:
  void SetupFeatureLists() override {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kHideShelfControlsInTabletMode,
          !navigation_buttons_shown_in_tablet_mode()},
         {features::kForestFeature, true}});
  }
};

class StackedHotseatWidgetTest : public HotseatWidgetTest {
 public:
  void SetupFeatureLists() override {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kHideShelfControlsInTabletMode,
          !navigation_buttons_shown_in_tablet_mode()}});
  }
};

// Counts the number of times the work area changes.
class DisplayWorkAreaChangeCounter : public display::DisplayObserver {
 public:
  DisplayWorkAreaChangeCounter() {
    Shell::Get()->display_manager()->AddDisplayObserver(this);
  }

  DisplayWorkAreaChangeCounter(const DisplayWorkAreaChangeCounter&) = delete;
  DisplayWorkAreaChangeCounter& operator=(const DisplayWorkAreaChangeCounter&) =
      delete;

  ~DisplayWorkAreaChangeCounter() override {
    Shell::Get()->display_manager()->RemoveDisplayObserver(this);
  }

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override {
    if (metrics & display::DisplayObserver::DISPLAY_METRIC_WORK_AREA)
      work_area_change_count_++;
  }

  int count() const { return work_area_change_count_; }

 private:
  int work_area_change_count_ = 0;
};

// Watches the shelf for state changes.
class ShelfStateWatcher : public ShelfObserver {
 public:
  ShelfStateWatcher() { AshTestBase::GetPrimaryShelf()->AddObserver(this); }
  ~ShelfStateWatcher() override {
    AshTestBase::GetPrimaryShelf()->RemoveObserver(this);
  }
  void OnShelfVisibilityStateChanged(ShelfVisibilityState new_state) override {
    state_change_count_++;
  }
  int state_change_count() const { return state_change_count_; }

 private:
  int state_change_count_ = 0;
};

// Watches the Hotseat transition animation states.
class HotseatTransitionAnimationObserver
    : public HotseatTransitionAnimator::Observer {
 public:
  explicit HotseatTransitionAnimationObserver(
      HotseatTransitionAnimator* hotseat_transition_animator)
      : hotseat_transition_animator_(hotseat_transition_animator) {
    hotseat_transition_animator_->AddObserver(this);
  }
  ~HotseatTransitionAnimationObserver() override {
    hotseat_transition_animator_->RemoveObserver(this);
  }

  // HotseatTransitionAnimtor::Observer:
  void OnHotseatTransitionAnimationWillStart(HotseatState from_state,
                                             HotseatState to_start) override {
    ++observer_counts_.started;
  }
  void OnHotseatTransitionAnimationEnded(HotseatState from_state,
                                         HotseatState to_start) override {
    ++observer_counts_.ended;
    if (run_loop_)
      run_loop_->Quit();
  }
  void OnHotseatTransitionAnimationAborted() override {
    ++observer_counts_.aborted;
  }

  void Wait() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void Reset() {
    if (run_loop_)
      run_loop_->Quit();
    observer_counts_ = {0};
  }

  // Checks that the started and ending/aborting methods have fired the same
  // amount of times.
  bool ObserverCountsEqual() const {
    return observer_counts_.started ==
           (observer_counts_.ended + observer_counts_.aborted);
  }

  int AnimationAbortedCalls() const { return observer_counts_.aborted; }

 private:
  // Struct which keeps track of the counts of the Observer method has fired.
  // These are used to verify that started calls = ended calls + aborted calls.
  struct ObserverCounts {
    int started;
    int ended;
    int aborted;
  } observer_counts_ = {0};
  std::unique_ptr<base::RunLoop> run_loop_;
  raw_ptr<HotseatTransitionAnimator> hotseat_transition_animator_;
};

// Used to test the Hotseat, ScrollableShelf, and DenseShelf features.
INSTANTIATE_TEST_SUITE_P(
    All,
    HotseatWidgetTest,
    testing::Combine(
        testing::Values(ShelfAutoHideBehavior::kNever,
                        ShelfAutoHideBehavior::kAlways),
        /*is_assistant_enabled*/ testing::Bool(),
        /*navigation_buttons_shown_in_tablet_mode*/ testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    All,
    HotseatWidgetForestTest,
    testing::Combine(
        testing::Values(ShelfAutoHideBehavior::kNever,
                        ShelfAutoHideBehavior::kAlways),
        /*is_assistant_enabled*/ testing::Bool(),
        /*navigation_buttons_shown_in_tablet_mode*/ testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    All,
    StackedHotseatWidgetTest,
    testing::Combine(
        testing::Values(ShelfAutoHideBehavior::kNever,
                        ShelfAutoHideBehavior::kAlways),
        /*is_assistant_enabled*/ testing::Bool(),
        /*navigation_buttons_shown_in_tablet_mode*/ testing::Bool()));

// TODO(b:270757104) Set status are widget sizes.
TEST_P(StackedHotseatWidgetTest, StackedHotseatShownOnSmallScreens) {
  UpdateDisplay("475x350");
  base::HistogramTester histogram_tester;
  // Nothing logged before entering the tablet mode.
  histogram_tester.ExpectTotalCount("Ash.Shelf.ShowStackedHotseat", 0);

  TabletModeControllerTestApi().EnterTabletMode();
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect hotseat_bounds = GetPrimaryShelf()
                                       ->shelf_widget()
                                       ->hotseat_widget()
                                       ->GetWindowBoundsInScreen();
  ASSERT_EQ(hotseat_bounds.bottom(),
            350 - ShelfConfig::Get()->hotseat_bottom_padding() * 2 -
                ShelfConfig::Get()->shelf_size());

  // Showed stacked hostseat.
  histogram_tester.ExpectBucketCount("Ash.Shelf.ShowStackedHotseat", true, 1);
  histogram_tester.ExpectBucketCount("Ash.Shelf.ShowStackedHotseat", false, 0);
}

// TODO(b:270757104) Set status are widget sizes.
TEST_P(StackedHotseatWidgetTest, StackedHotseatNotShownOnLargeScreens) {
  UpdateDisplay("800x600");

  base::HistogramTester histogram_tester;
  // Nothing logged before entering the tablet mode.
  histogram_tester.ExpectTotalCount("Ash.Shelf.ShowStackedHotseat", 0);

  TabletModeControllerTestApi().EnterTabletMode();
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect hotseat_bounds = GetPrimaryShelf()
                                       ->shelf_widget()
                                       ->hotseat_widget()
                                       ->GetWindowBoundsInScreen();
  ASSERT_EQ(hotseat_bounds.bottom(),
            600 - ShelfConfig::Get()->hotseat_bottom_padding());

  // Showed regular hotseat.
  histogram_tester.ExpectBucketCount("Ash.Shelf.ShowStackedHotseat", true, 0);
  histogram_tester.ExpectBucketCount("Ash.Shelf.ShowStackedHotseat", false, 1);
}

TEST_P(HotseatWidgetTest, LongPressHomeWithoutAppWindow) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();
  GetAppListTestHelper()->CheckVisibility(true);

  HotseatStateWatcher watcher(GetShelfLayoutManager());

  ShowShelfAndActivateAssistant();
  GetAppListTestHelper()->CheckVisibility(true);

  EXPECT_EQ(
      is_assistant_enabled(),
      GetAppListTestHelper()->GetAppListView()->IsShowingEmbeddedAssistantUI());

  // Hotseat should not change when showing Assistant.
  watcher.CheckEqual({});
}

TEST_P(HotseatWidgetTest, LongPressHomeWithAppWindow) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();
  GetAppListTestHelper()->CheckVisibility(true);

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  GetAppListTestHelper()->CheckVisibility(false);

  HotseatStateWatcher watcher(GetShelfLayoutManager());

  ShowShelfAndActivateAssistant();
  GetAppListTestHelper()->CheckVisibility(false);

  EXPECT_EQ(
      is_assistant_enabled(),
      GetAppListTestHelper()->GetAppListView()->IsShowingEmbeddedAssistantUI());

  std::vector<HotseatState> expected_state;
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways) {
    // |ShowShelfAndActivateAssistant()| will bring up shelf so it will trigger
    // one hotseat state change.
    expected_state.push_back(HotseatState::kExtended);
    // Launching the assistant from a shelf button on an autohidden shelf will
    // hide the shelf at the end of the operation.
    if (is_assistant_enabled() && navigation_buttons_shown_in_tablet_mode())
      expected_state.push_back(HotseatState::kHidden);
  }
  watcher.CheckEqual(expected_state);
}

// Tests that closing a window which was opened prior to entering tablet mode
// results in a kShownHomeLauncher hotseat.
TEST_P(HotseatWidgetTest, ClosingLastWindowInTabletMode) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  // Activate the window and go to tablet mode.
  wm::ActivateWindow(window.get());
  TabletModeControllerTestApi().EnterTabletMode();

  // Close the window, the AppListView should be shown, and the hotseat should
  // be kShownHomeLauncher.
  window->Hide();

  EXPECT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the hotseat is kShownHomeLauncher when entering tablet mode with
// no windows.
TEST_P(HotseatWidgetTest, GoingToTabletModeNoWindows) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());
}

// Tests that the hotseat is kHidden when entering tablet mode with a window.
TEST_P(HotseatWidgetTest, GoingToTabletModeWithWindows) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  // Activate the window and go to tablet mode.
  wm::ActivateWindow(window.get());
  TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  GetAppListTestHelper()->CheckVisibility(false);
}

// The in-app Hotseat should not be hidden automatically when the shelf context
// menu shows (https://crbug.com/1020388).
TEST_P(HotseatWidgetTest, InAppShelfShowingContextMenu) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible(
      display::Screen::GetScreen()->GetPrimaryDisplay().id()));

  ShelfTestUtil::AddAppShortcut("app_id", TYPE_PINNED_APP);

  // Swipe up on the shelf to show the hotseat.
  SwipeUpOnShelf();
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  ShelfViewTestAPI shelf_view_test_api(
      GetPrimaryShelf()->shelf_widget()->shelf_view_for_testing());
  ShelfAppButton* app_icon = shelf_view_test_api.GetButton(0);

  // Accelerate the generation of the long press event.
  ui::GestureConfiguration::GetInstance()->set_show_press_delay_in_ms(1);
  ui::GestureConfiguration::GetInstance()->set_short_press_time(
      base::Milliseconds(1));
  ui::GestureConfiguration::GetInstance()->set_long_press_time_in_ms(1);

  // Press the icon enough long time to generate the long press event.
  GetEventGenerator()->MoveTouch(app_icon->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressTouch();
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  const int long_press_delay_ms = gesture_config->long_press_time_in_ms() +
                                  gesture_config->show_press_delay_in_ms();
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::Milliseconds(long_press_delay_ms));
  run_loop.Run();
  GetEventGenerator()->ReleaseTouch();

  // Expects that the hotseat's state is kExntended.
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  // Ensures that the ink drop state is InkDropState::ACTIVATED before closing
  // the menu.
  app_icon->FireRippleActivationTimerForTest();
}

// Tests that a window that is created after going to tablet mode, then closed,
// results in a kShownHomeLauncher hotseat.
TEST_P(HotseatWidgetTest, CloseLastWindowOpenedInTabletMode) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  // Activate the window after entering tablet mode.
  wm::ActivateWindow(window.get());

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  GetAppListTestHelper()->CheckVisibility(false);

  // Hide the window, the hotseat should be kShownHomeLauncher, and the home
  // launcher should be visible.
  window->Hide();

  EXPECT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Verifies removing a shelf item by dragging it off the extended hotseat.
TEST_P(HotseatWidgetTest, DragItemOffExtendedHotseat) {
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  ShelfTestUtil::AddAppShortcut("app_id_1", TYPE_PINNED_APP);
  ShelfTestUtil::AddAppShortcut("app_id_2", TYPE_PINNED_APP);

  ShelfView* shelf_view = GetPrimaryShelf()
                              ->hotseat_widget()
                              ->scrollable_shelf_view()
                              ->shelf_view();
  EXPECT_EQ(2u, shelf_view->view_model_for_test()->view_size());

  // Show the in-app shelf.
  SwipeUpOnShelf();
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  // Start mouse drag on a shelf item.
  ShelfAppButton* dragged_button =
      ShelfViewTestAPI(shelf_view).GetButton(/*index=*/0);
  GetEventGenerator()->MoveMouseTo(
      dragged_button->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressLeftButton();
  EXPECT_TRUE(dragged_button->FireDragTimerForTest());
  EXPECT_TRUE(shelf_view->drag_view());

  // Move mouse. Verify that the hotseat is still extended.
  GetEventGenerator()->MoveMouseBy(/*x=*/0, /*y=*/-80);
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  // Release the mouse press. Verify that:
  // 1. Shelf item count decreases by one; and
  // 2. Hotseat is still extended.
  GetEventGenerator()->ReleaseLeftButton();
  EXPECT_EQ(1u, shelf_view->view_model_for_test()->view_size());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
}

// Tests that swiping up on an autohidden shelf shows the hotseat, and swiping
// down hides it.
TEST_P(HotseatWidgetTest, ShowingAndHidingAutohiddenShelf) {
  if (shelf_auto_hide_behavior() != ShelfAutoHideBehavior::kAlways)
    return;

  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  SwipeUpOnShelf();

  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, GetPrimaryShelf()->GetAutoHideState());

  SwipeDownOnShelf();

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, GetPrimaryShelf()->GetAutoHideState());

  // Swipe down again, nothing should change.
  SwipeDownOnShelf();

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, GetPrimaryShelf()->GetAutoHideState());
}

// Tests that swiping up on several places in the in-app shelf shows the
// hotseat (crbug.com/1016931).
TEST_P(HotseatWidgetTest, SwipeUpInAppShelfShowsHotseat) {
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 0);

  // Swipe up from the center of the shelf.
  SwipeUpOnShelf();
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);

  // Swipe down from the hotseat to hide it.
  gfx::Rect hotseat_bounds =
      GetPrimaryShelf()->hotseat_widget()->GetWindowBoundsInScreen();
  gfx::Point start = hotseat_bounds.top_center();
  gfx::Point end = start + gfx::Vector2d(0, 80);
  const base::TimeDelta kTimeDelta = base::Milliseconds(100);
  const int kNumScrollSteps = 4;

  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);
  ASSERT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 1);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);

  // Swipe up from the right part of the shelf (the system tray).
  start = GetShelfWidget()
              ->status_area_widget()
              ->GetWindowBoundsInScreen()
              .CenterPoint();
  end = start + gfx::Vector2d(0, -80);

  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 1);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 2);

  // Swipe down from the hotseat to hide it.
  start = hotseat_bounds.top_center();
  end = start + gfx::Vector2d(0, 80);

  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);
  ASSERT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 2);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 2);

  // Swipe up from the left part of the shelf (the home/back button).
  start = GetShelfWidget()
              ->navigation_widget()
              ->GetWindowBoundsInScreen()
              .CenterPoint();
  end = start + gfx::Vector2d(0, -80);

  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 2);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 3);
}

// Tests that swiping up on the hotseat does nothing.
TEST_P(HotseatWidgetTest, SwipeUpOnHotseatBackgroundDoesNothing) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 0);

  // Swipe up on the shelf to show the hotseat.
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible(
      display::Screen::GetScreen()->GetPrimaryDisplay().id()));

  SwipeUpOnShelf();

  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways)
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, GetPrimaryShelf()->GetAutoHideState());

  // Swipe up on the Hotseat (parent of ShelfView) does nothing.
  gfx::Point start(GetPrimaryShelf()
                       ->shelf_widget()
                       ->hotseat_widget()
                       ->GetWindowBoundsInScreen()
                       .top_center());
  const gfx::Point end(start + gfx::Vector2d(0, -300));
  const base::TimeDelta kTimeDelta = base::Milliseconds(100);
  const int kNumScrollSteps = 4;
  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);

  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible(
      display::Screen::GetScreen()->GetPrimaryDisplay().id()));
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways)
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, GetPrimaryShelf()->GetAutoHideState());
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);
}

// Tests that tapping an active window with an extended hotseat results in a
// hidden hotseat.
TEST_P(HotseatWidgetTest, TappingActiveWindowHidesHotseat) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 0);
  histogram_tester.ExpectBucketCount(
      kHotseatGestureHistogramName,
      InAppShelfGestures::kHotseatHiddenDueToInteractionOutsideOfShelf, 0);

  // Swipe up on the shelf to show the hotseat.
  SwipeUpOnShelf();

  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);
  histogram_tester.ExpectBucketCount(
      kHotseatGestureHistogramName,
      InAppShelfGestures::kHotseatHiddenDueToInteractionOutsideOfShelf, 0);

  // Tap the shelf background, nothing should happen.
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  gfx::Point tap_point = display_bounds.bottom_center();
  GetEventGenerator()->GestureTapAt(tap_point);

  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways)
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, GetPrimaryShelf()->GetAutoHideState());

  // Tap the active window, the hotseat should hide.
  tap_point.Offset(0, -200);
  GetEventGenerator()->GestureTapAt(tap_point);

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways)
    EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, GetPrimaryShelf()->GetAutoHideState());

  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);
  histogram_tester.ExpectBucketCount(
      kHotseatGestureHistogramName,
      InAppShelfGestures::kHotseatHiddenDueToInteractionOutsideOfShelf, 1);
}

// Tests that gesture dragging an active window hides the hotseat.
TEST_P(HotseatWidgetTest, GestureDraggingActiveWindowHidesHotseat) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 0);

  // Swipe up on the shelf to show the hotseat.
  SwipeUpOnShelf();

  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);

  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways)
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, GetPrimaryShelf()->GetAutoHideState());

  // Gesture drag on the active window, the hotseat should hide.
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  gfx::Point start = display_bounds.bottom_center();
  start.Offset(0, -200);
  gfx::Point end = start;
  end.Offset(0, -200);
  GetEventGenerator()->GestureScrollSequence(start, end, base::Milliseconds(10),
                                             4);

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways)
    EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, GetPrimaryShelf()->GetAutoHideState());

  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);
}

// Tests that a swipe up on the shelf shows the hotseat while in split view.
TEST_P(HotseatWidgetTest, SwipeUpOnShelfShowsHotseatInSplitView) {
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  std::unique_ptr<aura::Window> window2 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 0);

  // Go into split view mode by first going into overview, and then snapping
  // the open window on one side.
  EnterOverview();
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(window2.get(), SnapPosition::kSecondary);
  EXPECT_EQ(split_view_controller->state(),
            SplitViewController::State::kBothSnapped);

  // We should still be able to drag up the hotseat.
  SwipeUpOnShelf();
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);
}

// Tests that HotseatTransitionAimationObserver starting and ending calls have a
// 1:1 relation. This test verifies that behavior.
TEST_P(HotseatWidgetTest, ObserverCallsMatch) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  // Enter tablet mode to show the home launcher. Hotseat state should be
  // kShownHomeLauncher.
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());

  // Create a window to transition to the in-app shelf. Hotseat state should be
  // kHidden.
  HotseatTransitionAnimationObserver observer(
      GetPrimaryShelf()->shelf_widget()->hotseat_transition_animator());
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 800, 800));
  observer.Wait();
  EXPECT_TRUE(observer.ObserverCountsEqual());
  ASSERT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  observer.Reset();
  // Go to home launcher again. Hotseat state should be kShownHomeLauncher.
  ShowShelfAndGoHome();
  observer.Wait();
  EXPECT_TRUE(observer.ObserverCountsEqual());
  ASSERT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());

  observer.Reset();
  // Go to overview and cancel immediately. Hotseat state should be
  // kShownHomeLauncher.
  StartOverview();
  EXPECT_TRUE(OverviewController::Get()->IsInStartAnimation());
  // No animations should have been started so no animations are in progress
  // or aborted.
  EXPECT_TRUE(observer.ObserverCountsEqual());
  EXPECT_EQ(0, observer.AnimationAbortedCalls());

  EndOverview();

  // No animations should have been started or aborted.
  EXPECT_EQ(0, observer.AnimationAbortedCalls());
  EXPECT_TRUE(observer.ObserverCountsEqual());
  ASSERT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());

  observer.Reset();
  // Go to overview. Hotseat state should be kExtended.
  StartOverview();

  ASSERT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());
  EXPECT_TRUE(observer.ObserverCountsEqual());
}

// Tests that a swipe up on the shelf shows the hotseat while in split view.
TEST_P(HotseatWidgetTest, DisableBlurDuringOverviewMode) {
  // TODO(sammiequon): Remove this test when forest feature can no longer be
  // disabled.
  if (features::IsForestFeatureEnabled()) {
    return;
  }

  TabletModeControllerTestApi().EnterTabletMode();

  ASSERT_EQ(
      ShelfConfig::Get()->shelf_blur_radius(),
      GetShelfWidget()->hotseat_widget()->GetHotseatBackgroundBlurForTest());

  // Go into overview and check that at the end of the animation, background
  // blur is disabled.
  StartOverview();
  WaitForOverviewAnimation(/*enter=*/true);
  EXPECT_EQ(
      0, GetShelfWidget()->hotseat_widget()->GetHotseatBackgroundBlurForTest());

  // Exit overview and check that at the end of the animation, background
  // blur is enabled again.
  EndOverview();
  WaitForOverviewAnimation(/*enter=*/false);
  EXPECT_EQ(
      ShelfConfig::Get()->shelf_blur_radius(),
      GetShelfWidget()->hotseat_widget()->GetHotseatBackgroundBlurForTest());
}

TEST_P(HotseatWidgetForestTest, EnableBlurDuringOverviewMode) {
  TabletModeControllerTestApi().EnterTabletMode();

  const int expected_blur_radius = ShelfConfig::Get()->shelf_blur_radius();
  ASSERT_EQ(
      GetShelfWidget()->hotseat_widget()->GetHotseatBackgroundBlurForTest(),
      expected_blur_radius);

  // Go into overview and check that at the end of the animation, background
  // blur is still enabled.
  StartOverview();
  WaitForOverviewAnimation(/*enter=*/true);
  EXPECT_EQ(
      GetShelfWidget()->hotseat_widget()->GetHotseatBackgroundBlurForTest(),
      expected_blur_radius);

  // Exit overview and check that at the end of the animation, background
  // blur is still enabled.
  EndOverview();
  WaitForOverviewAnimation(/*enter=*/false);
  EXPECT_EQ(
      GetShelfWidget()->hotseat_widget()->GetHotseatBackgroundBlurForTest(),
      expected_blur_radius);
}

// Tests that releasing the hotseat gesture below the threshold results in a
// kHidden hotseat when the shelf is shown.
TEST_P(HotseatWidgetTest, ReleasingSlowDragBelowThreshold) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 0);

  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start(display_bounds.bottom_center());
  const int hotseat_size = GetPrimaryShelf()
                               ->shelf_widget()
                               ->hotseat_widget()
                               ->GetWindowBoundsInScreen()
                               .height();
  const gfx::Point end(start + gfx::Vector2d(0, -hotseat_size / 2 + 1));
  const base::TimeDelta kTimeDelta = base::Milliseconds(1000);
  const int kNumScrollSteps = 4;
  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 0);
}

// Tests that releasing the hotseat gesture above the threshold results in a
// kExtended hotseat.
TEST_P(HotseatWidgetTest, ReleasingSlowDragAboveThreshold) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 0);

  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start(display_bounds.bottom_center());
  const int hotseat_size = GetPrimaryShelf()
                               ->shelf_widget()
                               ->hotseat_widget()
                               ->GetWindowBoundsInScreen()
                               .height();
  const gfx::Point end(start + gfx::Vector2d(0, -hotseat_size * 3.0f / 2.0f));
  const base::TimeDelta kTimeDelta = base::Milliseconds(1000);
  const int kNumScrollSteps = 4;
  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);

  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways)
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, GetPrimaryShelf()->GetAutoHideState());
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);
}

// Tests that releasing the hotseat gesture when a stylus app is active has a
// bigger thresehold than normal apps.
TEST_P(HotseatWidgetTest, HotseatDragGestureForStylusApp) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  // Taken from ShelfLayoutManager.
  const int kShelfPalmRejectionSwipeOffset = 80;
  const std::string stylus_app = "fhapgmpiiiigioilnjmkiohjhlegnceb";

  ShelfModel* model = Shell::Get()->shelf_controller()->model();
  const ShelfID test_stylus_app_id(stylus_app);

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  window->SetProperty(kShelfIDKey, test_stylus_app_id.Serialize());
  wm::ActivateWindow(window.get());

  EXPECT_EQ(test_stylus_app_id, model->active_shelf_id());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 0);

  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start(display_bounds.bottom_center());
  const int hotseat_size = GetPrimaryShelf()
                               ->shelf_widget()
                               ->hotseat_widget()
                               ->GetWindowBoundsInScreen()
                               .height();
  const gfx::Point normal_thereshold(
      start + gfx::Vector2d(0, -hotseat_size * 3.0f / 2.0f));
  const base::TimeDelta kTimeDelta = base::Milliseconds(1000);
  const int kNumScrollSteps = 4;
  GetEventGenerator()->GestureScrollSequence(start, normal_thereshold,
                                             kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  const gfx::Point offset_thereshold(
      normal_thereshold + gfx::Vector2d(0, -kShelfPalmRejectionSwipeOffset));
  GetEventGenerator()->GestureScrollSequence(start, offset_thereshold,
                                             kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways)
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, GetPrimaryShelf()->GetAutoHideState());
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeDownToHide, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);
}

// Tests that showing overview after showing the hotseat results in only one
// animation, to |kExtended|.
TEST_P(HotseatWidgetTest, ShowingOverviewFromShownAnimatesOnce) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  auto state_watcher =
      std::make_unique<HotseatStateWatcher>(GetShelfLayoutManager());
  SwipeUpOnShelf();
  ASSERT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  StartOverview();
  state_watcher->CheckEqual({HotseatState::kExtended, HotseatState::kHidden});
}

// Tests that the hotseat is not flush with the bottom of the screen when home
// launcher is showing.
TEST_P(HotseatWidgetTest, HotseatNotFlushWhenHomeLauncherShowing) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();
  const int display_height =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().height();
  const int hotseat_bottom = GetPrimaryShelf()
                                 ->shelf_widget()
                                 ->hotseat_widget()
                                 ->GetWindowBoundsInScreen()
                                 .bottom();
  EXPECT_LT(hotseat_bottom, display_height);
}

// Tests that home -> overview results in only one hotseat state change.
TEST_P(HotseatWidgetTest, HomeToOverviewChangesStateOnce) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  // First, try with no windows open.
  {
    HotseatStateWatcher watcher(GetShelfLayoutManager());
    StartOverview();
    WaitForOverviewAnimation(/*enter=*/true);
    watcher.CheckEqual({/* shelf state should not change*/});
  }

  // Open a window, then open the home launcher.
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  ShowShelfAndGoHome();
  GetAppListTestHelper()->CheckVisibility(true);

  // Activate overview and expect the hotseat only changes state to extended.
  {
    HotseatStateWatcher watcher(GetShelfLayoutManager());
    StartOverview();
    WaitForOverviewAnimation(/*enter=*/true);

    watcher.CheckEqual({/* shelf state should not change*/});
  }
}

// Verifies that the hotseat widget and the status area widget are animated to
// the target location when entering overview mode in home launcher
// (https://crbug.com/1079347).
TEST_P(HotseatWidgetTest, VerifyShelfAnimationWhenEnteringOverview) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  HotseatWidget* hotseat_widget = GetPrimaryShelf()->hotseat_widget();
  ASSERT_EQ(HotseatState::kShownHomeLauncher, hotseat_widget->state());

  ui::LayerAnimator* hotseat_layer_animator =
      hotseat_widget->GetNativeView()->layer()->GetAnimator();
  ui::LayerAnimator* status_area_layer_animator = GetShelfWidget()
                                                      ->status_area_widget()
                                                      ->GetNativeView()
                                                      ->layer()
                                                      ->GetAnimator();
  ASSERT_FALSE(hotseat_layer_animator->is_animating());
  ASSERT_FALSE(status_area_layer_animator->is_animating());

  StartOverview();
  EXPECT_FALSE(hotseat_layer_animator->is_animating());
  EXPECT_FALSE(status_area_layer_animator->is_animating());
  ASSERT_EQ(HotseatState::kShownHomeLauncher, hotseat_widget->state());
}

// Tests that home -> in-app results in only one state change.
TEST_P(HotseatWidgetTest, HomeToInAppChangesStateOnce) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  // Go to in-app, the hotseat should hide.
  HotseatStateWatcher watcher(GetShelfLayoutManager());
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  watcher.CheckEqual({HotseatState::kHidden});
}

// Tests that in-app -> home via closing the only window, swiping from the
// bottom of the shelf, and tapping the home launcher button results in only one
// state change.
TEST_P(HotseatWidgetTest, InAppToHomeChangesStateOnce) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  // Go to in-app with an extended hotseat.
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  SwipeUpOnShelf();

  // Press the home button, the hotseat should transition directly to
  // kShownHomeLauncher.
  {
    HotseatStateWatcher watcher(GetShelfLayoutManager());
    ShowShelfAndGoHome();
    watcher.CheckEqual({HotseatState::kShownHomeLauncher});
  }
  // Go to in-app.
  window->Show();
  wm::ActivateWindow(window.get());

  // Extend the hotseat, then Swipe up to go home, the hotseat should transition
  // directly to kShownHomeLauncher.
  SwipeUpOnShelf();
  {
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    HotseatStateWatcher watcher(GetShelfLayoutManager());
    FlingUpOnShelf();
    watcher.CheckEqual({HotseatState::kShownHomeLauncher});

    // Wait for the window animation to complete, and verify the hotseat state
    // remained kShownHomeLauncher.
    ShellTestApi().WaitForWindowFinishAnimating(window.get());
    watcher.CheckEqual({HotseatState::kShownHomeLauncher});
  }

  // Nothing left to test for autohidden shelf.
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways)
    return;

  // Go to in-app and do not extend the hotseat.
  window->Show();
  wm::ActivateWindow(window.get());

  // TODO(manucornet): This is flaky when the shelf is always auto-hidden.
  // Investigate and fix (sometimes fails when the assistant is enabled,
  // sometimes not).
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kNever)
    return;

  // Press the home button, the hotseat should transition directly to
  // kShownHomeLauncher.
  {
    HotseatStateWatcher watcher(GetShelfLayoutManager());
    ShowShelfAndGoHome();
    watcher.CheckEqual({HotseatState::kShownHomeLauncher});
  }
}

// Tests that transitioning from overview to home while a transition from home
// to overview is still in progress ends up with hotseat in kShownHomeLauncher
// state (and in app shelf not visible).
TEST_P(HotseatWidgetTest, HomeToOverviewAndBack) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  WindowState::Get(window.get())->Minimize();

  HotseatStateWatcher watcher(GetShelfLayoutManager());

  // Start going to overview.
  {
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    StartOverview();

    watcher.CheckEqual({/*Hotseat state should not change*/});
  }

  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  ShowShelfAndGoHome();

  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(ShelfConfig::Get()->is_in_app());

  watcher.CheckEqual({/*Hotseat state should not change*/});
}

TEST_P(HotseatWidgetTest, InAppToOverviewAndBack) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window = CreateAppWindow(gfx::Rect(400, 400));

  // Make sure shelf (and overview button) are visible - this is moves the
  // hotseat into kExtended state.
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways) {
    SwipeUpOnShelf();
  }

  // Start going to overview - use non zero animation so transition is not
  // immediate.
  {
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    StartOverview();
  }

  // Start watching hotseat state after entering overview, so hotseat
  // change expectation match for both auto-hidden and always-shown shelf.
  HotseatStateWatcher watcher(GetShelfLayoutManager());

  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetAppListTestHelper()->CheckVisibility(false);

  // Hotseat should be hidden as overview is starting.
  watcher.CheckEqual({});

  // Exit overview to go back to the app window.
  EndOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(ShelfConfig::Get()->is_in_app());

  // The hotseat is expected to be hidden.
  watcher.CheckEqual({});
}

// Tests transition to home screen initiated while transition from app window to
// overview is in progress.
TEST_P(HotseatWidgetTest, ShowShelfAndGoHomeDuringInAppToOverviewTransition) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window = CreateAppWindow(gfx::Rect(400, 400));

  // Make sure shelf (and overview button) are visible - this is moves the
  // hotseat into kExtended state.
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways) {
    SwipeUpOnShelf();
  }

  // Start going to overview - use non zero animation so transition is not
  // immediate.
  {
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    StartOverview();
  }

  // Start watching hotseat state after entering overview, so hotseat
  // change expectation match for both auto-hidden and always-shown shelf.
  HotseatStateWatcher watcher(GetShelfLayoutManager());

  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetAppListTestHelper()->CheckVisibility(false);

  // Hotseat should be hidden as overview is starting.
  watcher.CheckEqual({});

  // Go home - expect transition to home (with hotseat in kShownHomeLauncher
  // state, and in app shelf hidden).
  ShowShelfAndGoHome();

  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(ShelfConfig::Get()->is_in_app());

  // If shelf is always hidden and navigation buttons are shown, we swipe up on
  // the shelf, extending the hotseat (see `ShowShelfAndGoHome()`).
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways &&
      navigation_buttons_shown_in_tablet_mode()) {
    watcher.CheckEqual(
        {HotseatState::kExtended, HotseatState::kShownHomeLauncher});
  } else {
    watcher.CheckEqual({HotseatState::kShownHomeLauncher});
  }
}

// Tests that in-app -> overview results in only one state change with an
// autohidden shelf.
TEST_P(HotseatWidgetTest, InAppToOverviewChangesStateOnceAutohiddenShelf) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  TabletModeControllerTestApi().EnterTabletMode();

  // Test going to overview mode using the controller from an autohide hidden
  // shelf. Go to in-app.
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  {
    HotseatStateWatcher watcher(GetShelfLayoutManager());
    // Enter overview by using the controller.
    EnterOverview();
    WaitForOverviewAnimation(/*enter=*/true);

    watcher.CheckEqual({});
  }

  ExitOverview();
  WaitForOverviewAnimation(/*enter=*/false);

  // Test in-app -> overview again with the autohide shown shelf.
  EXPECT_TRUE(ShelfConfig::Get()->is_in_app());
  EXPECT_EQ(ShelfAutoHideState::SHELF_AUTO_HIDE_HIDDEN,
            GetShelfLayoutManager()->auto_hide_state());
  SwipeUpOnShelf();
  {
    HotseatStateWatcher watcher(GetShelfLayoutManager());
    // Enter overview by using the controller.
    EnterOverview();
    WaitForOverviewAnimation(/*enter=*/true);

    watcher.CheckEqual({HotseatState::kHidden});
    EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  }
}

// Tests that going between Applist and overview in tablet mode with no windows
// results in no work area change.
TEST_P(HotseatWidgetTest,
       WorkAreaDoesNotUpdateAppListToFromOverviewWithNoWindow) {
  TabletModeControllerTestApi().EnterTabletMode();
  DisplayWorkAreaChangeCounter counter;

  EnterOverview();
  WaitForOverviewAnimation(/*enter=*/true);
  EXPECT_EQ(0, counter.count());

  EnterOverview();
  WaitForOverviewAnimation(/*enter=*/true);
  EXPECT_EQ(0, counter.count());
}

// Tests that switching between AppList and overview with a window results in no
// work area change.
TEST_P(HotseatWidgetTest,
       WorkAreaDoesNotUpdateAppListToFromOverviewWithWindow) {
  DisplayWorkAreaChangeCounter counter;
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  ASSERT_EQ(1, counter.count());
  ShowShelfAndGoHome();

  StartOverview();
  WaitForOverviewAnimation(/*enter=*/true);
  EXPECT_EQ(1, counter.count());

  EndOverview();
  WaitForOverviewAnimation(/*enter=*/false);
  EXPECT_EQ(1, counter.count());
}

// Tests that switching between AppList and an active window does not update the
// work area.
TEST_P(HotseatWidgetTest, WorkAreaDoesNotUpdateOpenWindowToFromAppList) {
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  ASSERT_TRUE(ShelfConfig::Get()->is_in_app());

  // Go to the home launcher, work area should not update.
  DisplayWorkAreaChangeCounter counter;
  ShowShelfAndGoHome();

  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(0, counter.count());

  // Go back to the window, work area should not update.
  wm::ActivateWindow(window.get());

  EXPECT_TRUE(ShelfConfig::Get()->is_in_app());
  EXPECT_EQ(0, counter.count());
}

// Tests that switching between overview and an active window does not update
// the work area.
TEST_P(HotseatWidgetTest, WorkAreaDoesNotUpdateOpenWindowToFromOverview) {
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  ASSERT_TRUE(ShelfConfig::Get()->is_in_app());

  // Go to overview, there should not be a work area update.
  DisplayWorkAreaChangeCounter counter;
  StartOverview();
  WaitForOverviewAnimation(/*enter=*/true);
  EXPECT_EQ(0, counter.count());

  // Go back to the app, there should not be a work area update.
  wm::ActivateWindow(window.get());

  EXPECT_TRUE(ShelfConfig::Get()->is_in_app());
  EXPECT_EQ(0, counter.count());
}

// Tests that the shelf opaque background is properly updated after a tablet
// mode transition with no apps.
TEST_P(HotseatWidgetTest, ShelfBackgroundNotVisibleInTabletModeNoApps) {
  TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
}

// Tests that the shelf opaque background is properly updated after a tablet
// mode transition with no apps with dense shelf.
TEST_P(HotseatWidgetTest, DenseShelfBackgroundNotVisibleInTabletModeNoApps) {
  UpdateDisplay("300x1000");
  TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
}

// Tests that the hotseat is extended if focused with a keyboard.
TEST_P(HotseatWidgetTest, ExtendHotseatIfFocusedWithKeyboard) {
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  ASSERT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Focus the shelf. Hotseat should now show extended.
  GetPrimaryShelf()->shelf_focus_cycler()->FocusShelf(false /* last_element */);
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  // Focus the status area. Hotseat should now hide, as it was
  // automatically extended by focusing it.
  GetPrimaryShelf()->shelf_focus_cycler()->FocusStatusArea(
      false /* last_element */);
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Now swipe up to show the shelf and then focus it with the keyboard. Hotseat
  // should keep extended.
  SwipeUpOnShelf();
  GetPrimaryShelf()->shelf_focus_cycler()->FocusShelf(false /* last_element */);
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  // Now focus the status area widget again. Hotseat should remain shown, as it
  // was manually extended.
  GetPrimaryShelf()->shelf_focus_cycler()->FocusStatusArea(
      false /* last_element */);
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
}

// Tests that if the hotseat was hidden while being focused, doing a traversal
// focus on the next element brings it up again.
TEST_P(HotseatWidgetTest, SwipeDownOnFocusedHotseat) {
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  ShelfTestUtil::AddAppShortcut("app_id_1", TYPE_APP);
  ShelfTestUtil::AddAppShortcut("app_id_2", TYPE_APP);
  ASSERT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Focus the shelf, then swipe down on the shelf to hide it. Hotseat should be
  // hidden.
  GetPrimaryShelf()->shelf_focus_cycler()->FocusShelf(false /* last_element */);
  gfx::Rect hotseat_bounds =
      GetPrimaryShelf()->hotseat_widget()->GetWindowBoundsInScreen();
  gfx::Point start = hotseat_bounds.top_center();
  gfx::Point end = start + gfx::Vector2d(0, 80);
  GetEventGenerator()->GestureScrollSequence(
      start, end, base::Milliseconds(100), 4 /*scroll_steps*/);
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Focus to the next element in the hotseat. The hotseat should show again.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
}

// Tests that in overview, we can still exit by clicking on the hotseat if the
// point is not on the visible area.
TEST_P(HotseatWidgetTest, ExitOverviewWithClickOnHotseat) {
  std::unique_ptr<aura::Window> window1 = AshTestBase::CreateTestWindow();
  ShelfTestUtil::AddAppShortcut("app_id_1", TYPE_APP);

  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  ASSERT_FALSE(WindowState::Get(window1.get())->IsMinimized());

  // Enter overview, hotseat is hidden. Swipe up to extended it and then choose
  // the point to the farthest left. This point will not be visible.
  auto* overview_controller = OverviewController::Get();
  auto* hotseat_widget = GetPrimaryShelf()->hotseat_widget();
  EnterOverview();
  ASSERT_TRUE(overview_controller->InOverviewSession());
  ASSERT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  SwipeUpOnShelf();
  ASSERT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  gfx::Point far_left_point =
      hotseat_widget->GetWindowBoundsInScreen().left_center();

  // Tests that on clicking, we exit overview and all windows are minimized.
  GetEventGenerator()->set_current_screen_location(far_left_point);
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMinimized());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

// Hides the hotseat if the hotseat is in kExtendedMode and the system tray
// is about to show (see https://crbug.com/1028321).
TEST_P(HotseatWidgetTest, DismissHotseatWhenSystemTrayShows) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());

  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  SwipeUpOnShelf();
  ASSERT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  // Activates the system tray when hotseat is in kExtended mode and waits for
  // the update in system tray to finish.
  StatusAreaWidget* status_area_widget = GetShelfWidget()->status_area_widget();
  const gfx::Point status_area_widget_center =
      status_area_widget->GetNativeView()->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->GestureTapAt(status_area_widget_center);
  base::RunLoop().RunUntilIdle();

  // Expects that the system tray shows and the hotseat is hidden.
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  EXPECT_TRUE(status_area_widget->unified_system_tray()->IsBubbleShown());

  // Early out since the remaining code is only meaningful for auto-hide shelf.
  if (GetPrimaryShelf()->auto_hide_behavior() !=
      ShelfAutoHideBehavior::kAlways) {
    return;
  }

  // Auto-hide shelf should show when opening the system tray.
  EXPECT_EQ(ShelfAutoHideState::SHELF_AUTO_HIDE_SHOWN,
            GetShelfLayoutManager()->auto_hide_state());

  // Auto-hide shelf should hide when closing the system tray.
  GetEventGenerator()->GestureTapAt(status_area_widget_center);

  // Waits for the system tray to be closed.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ShelfAutoHideState::SHELF_AUTO_HIDE_HIDDEN,
            GetShelfLayoutManager()->auto_hide_state());
}

// Tests that the hotseat hides when it is in kExtendedMode and a status area
// tray bubble is shown.
TEST_P(HotseatWidgetTest, DismissHotseatWhenStatusAreaTrayShows) {
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  StatusAreaWidget* status_area_widget = GetShelfWidget()->status_area_widget();
  status_area_widget->ime_menu_tray()->SetVisiblePreferred(true);

  // Show the hotseat.
  SwipeUpOnShelf();
  ASSERT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  EXPECT_FALSE(status_area_widget->ime_menu_tray()->GetBubbleView());

  // Show the ime menu tray bubble, and wait for the hotseat to be hidden.
  GetEventGenerator()->GestureTapAt(
      status_area_widget->ime_menu_tray()->GetBoundsInScreen().CenterPoint());
  base::RunLoop().RunUntilIdle();

  // The hotseat should be hidden and the tray bubble should be shown.
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  EXPECT_TRUE(status_area_widget->ime_menu_tray()->GetBubbleView());

  // Swiping up on the shelf should hide the tray bubble and extend the hotseat.
  SwipeUpOnShelf();
  EXPECT_FALSE(status_area_widget->ime_menu_tray()->GetBubbleView());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
}

// Tests that the work area updates once each when going to/from tablet mode
// with no windows open.
TEST_P(HotseatWidgetTest,
       DISABLED_WorkAreaUpdatesClamshellToFromHomeLauncherNoWindows) {
  DisplayWorkAreaChangeCounter counter;
  TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_EQ(1, counter.count());

  TabletModeControllerTestApi().LeaveTabletMode();

  EXPECT_EQ(2, counter.count());
}

// Tests that the work area changes just once when opening a window in tablet
// mode.
TEST_P(HotseatWidgetTest, OpenWindowInTabletModeChangesWorkArea) {
  DisplayWorkAreaChangeCounter counter;
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_EQ(1, counter.count());

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  EXPECT_EQ(1, counter.count());
}

// Tests that going to and from tablet mode with an open window results in a
// work area change.
TEST_P(HotseatWidgetTest, ToFromTabletModeWithWindowChangesWorkArea) {
  DisplayWorkAreaChangeCounter counter;
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(1, counter.count());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(2, counter.count());
}

// Tests that the work area changes when fullscreening the active window or
// autohiding the shelf.
TEST_P(HotseatWidgetTest, ShelfVisibilityChangeChangesWorkArea) {
  UpdateDisplay("800x603");

  TabletModeControllerTestApi().EnterTabletMode();
  auto window = AshTestBase::CreateTestWindow(gfx::Rect(400, 400));

  // The expected work area is 3 pixels smaller to leave space to swipe the auto
  // hide shelf up.
  const gfx::Rect expected_auto_hide_work_area(800, 600);
  const gfx::Rect expected_in_app_work_area(
      800, 603 - ShelfConfig::Get()->in_app_shelf_size());
  auto get_work_area = []() -> gfx::Rect {
    return WorkAreaInsets::ForWindow(Shell::GetPrimaryRootWindow())
        ->user_work_area_bounds();
  };

  DisplayWorkAreaChangeCounter counter;
  WMEvent toggle_fullscreen(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window.get())->OnWMEvent(&toggle_fullscreen);
  EXPECT_EQ(expected_auto_hide_work_area, get_work_area());
  EXPECT_EQ(1, counter.count());

  WindowState::Get(window.get())->OnWMEvent(&toggle_fullscreen);
  EXPECT_EQ(expected_in_app_work_area, get_work_area());
  EXPECT_EQ(2, counter.count());

  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(expected_auto_hide_work_area, get_work_area());
  EXPECT_EQ(3, counter.count());

  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  EXPECT_EQ(expected_in_app_work_area, get_work_area());
  EXPECT_EQ(4, counter.count());
}

// Tests that the hotseat is flush with the bottom of the screen when in
// clamshell mode and the shelf is oriented on the bottom.
TEST_P(HotseatWidgetTest, HotseatFlushWithScreenBottomInClamshell) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  const int display_height =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().height();
  const int hotseat_bottom = GetPrimaryShelf()
                                 ->shelf_widget()
                                 ->hotseat_widget()
                                 ->GetWindowBoundsInScreen()
                                 .bottom();
  EXPECT_EQ(hotseat_bottom, display_height);
}

// Tests that upward drag gesture from the shelf in tablet mode affects the
// active window presentation.
TEST_P(HotseatWidgetTest, DragActiveWindowInTabletMode) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  // Swipe up to bring up the hotseat first.
  SwipeUpOnShelf();
  ASSERT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  // Now swipe up again to start drag the active window.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const gfx::Rect bottom_shelf_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  generator->MoveMouseTo(bottom_shelf_bounds.CenterPoint());
  generator->PressTouch();
  EXPECT_TRUE(window->layer()->transform().IsIdentity());

  // Drag upward, test the window transform changes.
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  generator->MoveTouch(display_bounds.CenterPoint());
  const gfx::Transform upward_transform = window->layer()->transform();
  EXPECT_FALSE(upward_transform.IsIdentity());
  // Drag downwad, test the window tranfrom changes.
  generator->MoveTouch(display_bounds.bottom_center());
  const gfx::Transform downward_transform = window->layer()->transform();
  EXPECT_NE(upward_transform, downward_transform);

  generator->ReleaseTouch();
  EXPECT_TRUE(window->layer()->transform().IsIdentity());
}

// Tests that when hotseat and drag-window-to-overview features are both
// enabled, hotseat is not extended after dragging a window to overview, and
// then activating the window.
TEST_P(HotseatWidgetTest, ExitingOverviewHidesHotseat) {
  const ShelfAutoHideBehavior auto_hide_behavior = shelf_auto_hide_behavior();
  GetPrimaryShelf()->SetAutoHideBehavior(auto_hide_behavior);
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  // If the shelf is auto-hidden, swipe up to bring up shelf and hotseat first
  // (otherwise, the window drag to overview will not be handled).
  if (auto_hide_behavior == ShelfAutoHideBehavior::kAlways) {
    SwipeUpOnShelf();
    ASSERT_EQ(HotseatState::kExtended,
              GetShelfLayoutManager()->hotseat_state());
  }

  // Swipe up to start dragging the active window.
  const gfx::Rect bottom_shelf_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  StartScroll(bottom_shelf_bounds.CenterPoint());
  // Ensure swipe goes past the top of the hotseat first to activate the window
  // drag controller.
  UpdateScroll(gfx::Vector2d(
      0, -GetPrimaryShelf()->hotseat_widget()->GetHotseatFullDragAmount()));
  // Drag upward, to the center of the screen, and release (this should enter
  // the overview).
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  UpdateScroll(gfx::Vector2d(0, display_bounds.CenterPoint().y() -
                                    bottom_shelf_bounds.CenterPoint().y()));
  // Small scroll update, to simulate the user holding the pointer.
  UpdateScroll(gfx::Vector2d(0, 2));
  DragWindowFromShelfController* window_drag_controller =
      GetShelfLayoutManager()->window_drag_controller_for_testing();
  ASSERT_TRUE(window_drag_controller);
  DragWindowFromShelfControllerTestApi test_api;
  test_api.WaitUntilOverviewIsShown(window_drag_controller);
  EndScroll(/*is_fling=*/false, 0.f);

  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Activate the window - the overview session should exit, and hotseat should
  // be hidden.
  wm::ActivateWindow(window.get());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
}

// Tests that failing to drag the maximized window to overview mode results in
// an extended hotseat.
TEST_P(HotseatWidgetTest, FailingOverviewDragResultsInExtendedHotseat) {
  const ShelfAutoHideBehavior auto_hide_behavior = shelf_auto_hide_behavior();
  GetPrimaryShelf()->SetAutoHideBehavior(auto_hide_behavior);
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  // If the shelf is auto-hidden, swipe up to bring up shelf and hotseat first
  // (otherwise, the window drag to overview will not be handled).
  if (auto_hide_behavior == ShelfAutoHideBehavior::kAlways) {
    SwipeUpOnShelf();
    ASSERT_EQ(HotseatState::kExtended,
              GetShelfLayoutManager()->hotseat_state());
  }

  // Swipe up to start dragging the active window.
  const gfx::Rect bottom_shelf_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  StartScroll(bottom_shelf_bounds.top_center());

  const int extended_hotseat_distance_from_top_of_shelf =
      ShelfConfig::Get()->hotseat_bottom_padding() +
      GetPrimaryShelf()->hotseat_widget()->GetHotseatSize();
  // Overview is triggered when the bottom of the dragged window goes past the
  // top of the hotseat. The window scaling and translation are handled slightly
  // differently for if the hotseat is extended or not.
  if (HotseatState::kExtended == GetShelfLayoutManager()->hotseat_state()) {
    // Drag upward, a bit below the hotseat extended height, to ensure that the
    // bottom of the dragged window doesn't go past the top of the hotseat, so
    // that it doesn't go into overview.
    UpdateScroll(
        gfx::Vector2d(0, -extended_hotseat_distance_from_top_of_shelf + 20));
  } else {
    // Drag upward, a bit past the hotseat extended height so that the window
    // drag controller is activated, but not enough to go to overview.
    UpdateScroll(
        gfx::Vector2d(0, -extended_hotseat_distance_from_top_of_shelf - 30));
  }
  EndScroll(/*is_fling=*/false, 0.f);

  ASSERT_FALSE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
}

// Tests that hotseat remains in extended state while in overview mode when
// flinging the shelf up or down.
TEST_P(HotseatWidgetTest, SwipeOnHotseatInOverview) {
  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  OverviewController* overview_controller = OverviewController::Get();
  EnterOverview();

  Shelf* const shelf = GetPrimaryShelf();

  SwipeUpOnShelf();

  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways) {
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  } else {
    EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  }

  // Drag from the hotseat to the bezel, the hotseat should remain in extended
  // state.
  DragHotseatDownToBezel();

  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways) {
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  } else {
    EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  }

  SwipeUpOnShelf();

  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways) {
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  } else {
    EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  }
}

TEST_P(HotseatWidgetTest, SwipeOnHotseatInSplitViewWithOverview) {
  Shelf* const shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  OverviewController* overview_controller = OverviewController::Get();
  EnterOverview();

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window.get(), SnapPosition::kPrimary);

  SwipeUpOnShelf();

  EXPECT_TRUE(split_view_controller->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways) {
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  } else {
    EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  }

  DragHotseatDownToBezel();

  EXPECT_TRUE(split_view_controller->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways) {
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  } else {
    EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  }

  SwipeUpOnShelf();

  EXPECT_TRUE(split_view_controller->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways) {
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  } else {
    EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  }
}

TEST_P(HotseatWidgetTest, SwipeOnHotseatInSplitView) {
  Shelf* const shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window1 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  std::unique_ptr<aura::Window> window2 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window1.get());

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(window2.get(), SnapPosition::kSecondary);
  EXPECT_TRUE(split_view_controller->InSplitViewMode());

  SwipeUpOnShelf();

  EXPECT_TRUE(split_view_controller->InSplitViewMode());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways) {
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  } else {
    EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  }

  DragHotseatDownToBezel();

  EXPECT_TRUE(split_view_controller->InSplitViewMode());
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways) {
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  } else {
    EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  }

  SwipeUpOnShelf();

  EXPECT_TRUE(split_view_controller->InSplitViewMode());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  if (shelf_auto_hide_behavior() == ShelfAutoHideBehavior::kAlways) {
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  } else {
    EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  }
}

// Tests that swiping downward, towards the bezel, from a variety of points
// results in hiding the hotseat.
TEST_P(HotseatWidgetTest, HotseatHidesWhenSwipedToBezel) {
  // Go to in-app shelf and extend the hotseat.
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  SwipeUpOnShelf();

  // Drag from the hotseat to the bezel, the hotseat should hide.
  DragHotseatDownToBezel();
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Reset the hotseat and swipe from the center of the hotseat, it should hide.
  SwipeUpOnShelf();

  gfx::Rect shelf_widget_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Rect hotseat_bounds =
      GetPrimaryShelf()->hotseat_widget()->GetWindowBoundsInScreen();
  gfx::Point start = hotseat_bounds.CenterPoint();
  const gfx::Point end =
      gfx::Point(shelf_widget_bounds.x() + shelf_widget_bounds.width() / 2,
                 shelf_widget_bounds.bottom() + 1);
  const base::TimeDelta kTimeDelta = base::Milliseconds(100);
  const int kNumScrollSteps = 4;

  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Reset the hotseat and swipe from the bottom of the hotseat, it should hide.
  SwipeUpOnShelf();

  start = hotseat_bounds.bottom_center();
  start.Offset(0, -1);
  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Reset the hotseat and swipe from the center of the in-app shelf, it should
  // hide.
  SwipeUpOnShelf();

  start = shelf_widget_bounds.CenterPoint();

  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Reset the hotseat and swipe from the bottom of the in-app shelf, it should
  // hide.
  SwipeUpOnShelf();

  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  start = shelf_widget_bounds.bottom_center();
  // The first few events which get sent to ShelfLayoutManager are
  // ui::EventType::kTapDown, and ui::EventType::kGestureStart. After a few px
  // we get ui::EventType::kGestureScrollUpdate. Add 6 px of slop to get the
  // first events out of the way, and 1 extra px to ensure we are not on the
  // bottom edge of the display.
  start.Offset(0, -7);

  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
}

// Tests that flinging up the in-app shelf should show the hotseat.
TEST_P(HotseatWidgetTest, FlingUpHotseatWithShortFling) {
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->CheckVisibility(false);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 0);

  // Scrolls the hotseat by a distance not sufficuent to trigger the action of
  // entering home screen from the in-app shelf.
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start(display_bounds.bottom_center());
  const gfx::Point end(start + gfx::Vector2d(0, -20));

  const int fling_speed =
      DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 1;
  const int scroll_steps = 20;
  base::TimeDelta scroll_time =
      GetEventGenerator()->CalculateScrollDurationForFlingVelocity(
          start, end, fling_speed, scroll_steps);
  GetEventGenerator()->GestureScrollSequence(start, end, scroll_time,
                                             scroll_steps);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  GetAppListTestHelper()->CheckVisibility(false);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);
}

// Tests that flinging up the in-app shelf should show the home launcher if the
// gesture distance is long enough.
TEST_P(HotseatWidgetTest, FlingUpHotseatWithLongFling) {
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->CheckVisibility(false);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kHotseatGestureHistogramName,
      InAppShelfGestures::kFlingUpToShowHomeScreen, 0);

  // Scrolls the hotseat by the sufficient distance to trigger the action of
  // entering home screen from the in-app shelf.
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start(display_bounds.bottom_center());
  const gfx::Point end(start + gfx::Vector2d(0, -200));

  const int fling_speed =
      DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 1;
  const int scroll_steps = 20;
  base::TimeDelta scroll_time =
      GetEventGenerator()->CalculateScrollDurationForFlingVelocity(
          start, end, fling_speed, scroll_steps);
  GetEventGenerator()->GestureScrollSequence(start, end, scroll_time,
                                             scroll_steps);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());
  GetAppListTestHelper()->CheckVisibility(true);
  histogram_tester.ExpectBucketCount(
      kHotseatGestureHistogramName,
      InAppShelfGestures::kFlingUpToShowHomeScreen, 1);
}

// Tests that UpdateVisibilityState is ignored during a shelf drag. This
// prevents drag from getting interrupted.
TEST_P(HotseatWidgetTest, NoVisibilityStateUpdateDuringDrag) {
  // Autohide the shelf, then start a shelf drag.
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  std::unique_ptr<aura::Window> window1 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window1.get());
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, GetPrimaryShelf()->GetAutoHideState());

  // Drag the autohidden shelf up a bit, then open a new window and activate it
  // during the drag. The shelf state should not change.
  gfx::Point start_drag = GetVisibleShelfWidgetBoundsInScreen().top_center();
  GetEventGenerator()->set_current_screen_location(start_drag);
  GetEventGenerator()->PressTouch();
  GetEventGenerator()->MoveTouchBy(0, -2);
  auto shelf_state_watcher = std::make_unique<ShelfStateWatcher>();
  std::unique_ptr<aura::Window> window2 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));

  wm::ActivateWindow(window2.get());
  window2->SetBounds(gfx::Rect(0, 0, 200, 200));

  EXPECT_EQ(0, shelf_state_watcher->state_change_count());
}

// Tests that when tablet mode has ended, the hotseat background is no longer
// visible. See crbug/1050383
TEST_P(HotseatWidgetTest, HotseatBackgroundDisappearsAfterTabletModeEnd) {
  HotseatWidget* hotseat = GetPrimaryShelf()->hotseat_widget();
  EXPECT_FALSE(hotseat->GetIsTranslucentBackgroundVisibleForTest());

  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(hotseat->GetIsTranslucentBackgroundVisibleForTest());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(hotseat->GetIsTranslucentBackgroundVisibleForTest());
}

// Tests that popups don't activate the hotseat. (crbug.com/1018266)
TEST_P(HotseatWidgetTest, HotseatRemainsHiddenIfPopupLaunched) {
  // Go to in-app shelf and extend the hotseat.
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  SwipeUpOnShelf();
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  // Hide hotseat by clicking outside its bounds.
  gfx::Rect hotseat_bounds =
      GetPrimaryShelf()->hotseat_widget()->GetWindowBoundsInScreen();
  gfx::Point start = hotseat_bounds.top_center();
  GetEventGenerator()->GestureTapAt(gfx::Point(start.x() + 1, start.y() - 1));
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Create a popup window and wait until all actions finish. The hotseat should
  // remain hidden.
  aura::Window* window_2 = CreateTestWindowInParent(window.get());
  window_2->SetBounds(gfx::Rect(201, 0, 100, 100));
  window_2->SetProperty(aura::client::kShowStateKey,
                        ui::mojom::WindowShowState::kNormal);
  window_2->Show();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
}

// Tests that blur is not showing during animations.
TEST_P(HotseatWidgetTest, NoBlurDuringAnimations) {
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_EQ(
      ShelfConfig::Get()->shelf_blur_radius(),
      GetShelfWidget()->hotseat_widget()->GetHotseatBackgroundBlurForTest());
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Open a window, as the hotseat animates to kHidden, it should lose its blur.
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(
      0, GetShelfWidget()->hotseat_widget()->GetHotseatBackgroundBlurForTest());

  // Wait for the animation to finish, hotseat blur should return.
  ShellTestApi().WaitForWindowFinishAnimating(window.get());
  EXPECT_EQ(
      ShelfConfig::Get()->shelf_blur_radius(),
      GetShelfWidget()->hotseat_widget()->GetHotseatBackgroundBlurForTest());
}

// Tests that hotseat bounds don't jump when transitioning from drag to
// animation.
TEST_P(HotseatWidgetTest, AnimationAfterDrag) {
  TabletModeControllerTestApi().EnterTabletMode();
  // Add an app to shelf - the app will be used to track the shelf view position
  // throughout the test.
  ShelfTestUtil::AddAppShortcut("fake_app", TYPE_PINNED_APP);

  // Open a window so the hotseat transitions to hidden state.
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  ui::ScopedAnimationDurationScaleMode animation_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  HotseatWidget* const hotseat_widget = GetPrimaryShelf()->hotseat_widget();
  gfx::Point last_app_views_position =
      hotseat_widget->GetWindowBoundsInScreen().origin();

  // Returns whether the hotseat vertical position has changed comapred to
  // |last_hotseat_y|, and updates |last_hotseat_y| to match the current hotseat
  // position.
  auto app_views_moved = [&last_app_views_position, &hotseat_widget]() -> bool {
    gfx::Point app_views_position =
        ShelfViewTestAPI(hotseat_widget->scrollable_shelf_view()->shelf_view())
            .GetViewAt(0)
            ->GetBoundsInScreen()
            .origin();
    app_views_position =
        hotseat_widget->GetLayer()->transform().MapPoint(app_views_position);
    const bool position_changed = app_views_position != last_app_views_position;
    last_app_views_position = app_views_position;
    return position_changed;
  };

  ui::test::EventGenerator* generator = GetEventGenerator();
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  // Drag upwards from the bottom of the screen to bring up hotseat - this
  // should request presentation time metric to be reported.
  generator->PressTouch(display_bounds.bottom_center());

  // Drag the hotseat upward, by the height of the shelf.
  const int shelf_height = GetShelfWidget()->GetWindowBoundsInScreen().height();

  generator->MoveTouchBy(0, -shelf_height);
  ASSERT_TRUE(app_views_moved());

  // Release touch, and verify the hotseat position remained the same.
  generator->ReleaseTouch();
  EXPECT_FALSE(app_views_moved());

  // Wait for the hotseat to animate to extended state.
  ShellTestApi().WaitForWindowFinishAnimating(
      hotseat_widget->GetNativeWindow());
  ASSERT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  EXPECT_TRUE(app_views_moved());

  // Drag hotseat down.
  generator->PressTouch(hotseat_widget->GetWindowBoundsInScreen().top_center());
  generator->MoveTouchBy(0, shelf_height);
  ASSERT_TRUE(app_views_moved());

  // Release touch, and verify the hotseat position remained the same.
  generator->ReleaseTouch();
  EXPECT_FALSE(app_views_moved());

  // Wait for the hotseat to animate to extended state.
  ShellTestApi().WaitForWindowFinishAnimating(
      hotseat_widget->GetNativeWindow());
  ASSERT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  ASSERT_TRUE(app_views_moved());

  // Drag the hotseat back up, and release so animation to extended state
  // starts.
  generator->PressTouch(display_bounds.bottom_center());
  generator->MoveTouchBy(0, -shelf_height);
  ASSERT_TRUE(app_views_moved());
  generator->ReleaseTouch();
  EXPECT_FALSE(app_views_moved());

  // Close the test widget to start transition to the shown state.
  window.reset();

  // The apparent hotseat bound should remain the same as the transition to show
  // state starts.
  ASSERT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());
  EXPECT_FALSE(app_views_moved());

  // Finish the animation to shown state.
  ShellTestApi().WaitForWindowFinishAnimating(
      hotseat_widget->GetNativeWindow());
  ASSERT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());
  EXPECT_TRUE(app_views_moved());
}

// Tests that hotseat bounds don't jump when the hotseat widget is translated
// when a transitionj animation starts.
TEST_P(HotseatWidgetTest, InitialAnimationPositionWithNonIdentityTransform) {
  TabletModeControllerTestApi().EnterTabletMode();
  // Add an app to shelf - the app will be used to track the shelf view position
  // throughout the test.
  ShelfTestUtil::AddAppShortcut("fake_app", TYPE_PINNED_APP);

  // Open a window so the hotseat transitions to hidden state.
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  // Make sure that all shelf item views complete their bounds animations
  // before starting the test (tests depend on the first item bounds within the
  // shelf view).
  auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
  ShelfViewTestAPI(shelf_view).RunMessageLoopUntilAnimationsDone();

  HotseatWidget* const hotseat_widget = GetPrimaryShelf()->hotseat_widget();
  gfx::Point last_app_views_position =
      hotseat_widget->GetWindowBoundsInScreen().origin();

  // Returns whether the hotseat vertical position has changed comapred to
  // |last_hotseat_y|, and updates |last_hotseat_y| to match the current hotseat
  // position.
  auto app_views_moved = [&last_app_views_position, &hotseat_widget]() -> bool {
    gfx::Point app_views_position =
        ShelfViewTestAPI(hotseat_widget->scrollable_shelf_view()->shelf_view())
            .GetViewAt(0)
            ->GetBoundsInScreen()
            .origin();
    app_views_position =
        hotseat_widget->GetLayer()->transform().MapPoint(app_views_position);
    const bool position_changed = app_views_position != last_app_views_position;
    last_app_views_position = app_views_position;
    return position_changed;
  };

  ui::test::EventGenerator* generator = GetEventGenerator();
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  // Drag upwards from the bottom of the screen to bring up hotseat - this
  // should request presentation time metric to be reported.
  generator->PressTouch(display_bounds.bottom_center());

  // Drag the hotseat upward, to transition hotseat to the extended state.
  const int shelf_height = GetShelfWidget()->GetWindowBoundsInScreen().height();
  generator->MoveTouchBy(0, -shelf_height);
  // Release touch, and verify the hotseat position remained the same.
  generator->ReleaseTouch();
  ASSERT_TRUE(app_views_moved());

  ui::ScopedAnimationDurationScaleMode animation_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  auto set_animated_transform = [](ui::Layer* layer,
                                   const gfx::Vector2d& initial_offset) {
    // Set translate animation on the hotseat widget, to simulate a state which
    // the widget may have while a transform animation is in progress.
    gfx::Transform initial_transform;
    initial_transform.Translate(initial_offset.x(), initial_offset.y());
    layer->SetTransform(initial_transform);

    // Set up an animation to identity transform.
    ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
    animation.SetTransitionDuration(base::Milliseconds(300));
    layer->SetTransform(gfx::Transform());
  };

  set_animated_transform(hotseat_widget->GetLayer(), gfx::Vector2d(0, -10));
  ASSERT_TRUE(app_views_moved());

  // Tap in the middle of the screen to initiate transition to hidden state.
  generator->GestureTapAt(display_bounds.CenterPoint());
  ASSERT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Verify the current hotseat position remains unchanged.
  EXPECT_FALSE(app_views_moved());

  // Wait for the hotseat animation to hidden state to finish.
  ShellTestApi().WaitForWindowFinishAnimating(
      hotseat_widget->GetNativeWindow());
  ASSERT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  EXPECT_TRUE(app_views_moved());

  // Set the hotseat widget transform again.
  set_animated_transform(hotseat_widget->GetLayer(), gfx::Vector2d(0, -10));
  ASSERT_TRUE(app_views_moved());

  // CLose the window to transition to the shown state.
  window.reset();
  ASSERT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());

  // The apparent hotseat bound should remain the same as the transition to show
  // state starts.
  EXPECT_FALSE(app_views_moved());

  // Finish the animation to shown state.
  ShellTestApi().WaitForWindowFinishAnimating(
      hotseat_widget->GetNativeWindow());
  EXPECT_TRUE(app_views_moved());

  // Open another widow, and move the hotseat to extended state.
  window = AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  generator->PressTouch(display_bounds.bottom_center());
  generator->MoveTouchBy(0, -shelf_height);
  generator->ReleaseTouch();
  ShellTestApi().WaitForWindowFinishAnimating(
      hotseat_widget->GetNativeWindow());
  ASSERT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  set_animated_transform(hotseat_widget->GetLayer(), gfx::Vector2d(0, -10));
  ASSERT_TRUE(app_views_moved());

  // Close the window to transition to shown hotseat state.
  window.reset();
  ASSERT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());

  // The apparent hotseat bound should remain the same as the transition to show
  // state starts.
  EXPECT_FALSE(app_views_moved());

  // Finish the animation to shown state.
  ShellTestApi().WaitForWindowFinishAnimating(
      hotseat_widget->GetNativeWindow());
  EXPECT_TRUE(app_views_moved());
}

TEST_P(HotseatWidgetTest, PresentationTimeMetricDuringDrag) {
  ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
      true);

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  GetPrimaryShelf()->SetAutoHideBehavior(shelf_auto_hide_behavior());
  TabletModeControllerTestApi().EnterTabletMode();

  ui::test::EventGenerator* generator = GetEventGenerator();
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  // Drag upwards from the bottom of the screen to bring up hotseat - this
  // should request presentation time metric to be reported.
  generator->PressTouch(display_bounds.bottom_center());

  HotseatWidget* const hotseat_widget = GetPrimaryShelf()->hotseat_widget();
  int last_hotseat_y = hotseat_widget->GetWindowBoundsInScreen().y();

  // Returns whether the hotseat vertical position has changed comapred to
  // |last_hotseat_y|, and updates |last_hotseat_y| to match the current hotseat
  // position.
  auto hotseat_moved = [&last_hotseat_y, &hotseat_widget]() -> bool {
    const int hotseat_y = hotseat_widget->GetWindowBoundsInScreen().y();
    const bool y_changed = hotseat_y != last_hotseat_y;
    last_hotseat_y = hotseat_y;
    return y_changed;
  };

  base::HistogramTester histogram_tester;

  auto check_bucket_size = [&histogram_tester](int expected_size) {
    histogram_tester.ExpectTotalCount(
        "Ash.HotseatTransition.Drag.PresentationTime", expected_size);
    histogram_tester.ExpectTotalCount(
        "Ash.HotseatTransition.Drag.PresentationTime.MaxLatency", 0);
  };

  int expected_bucket_size = 0;
  {
    SCOPED_TRACE("Initial state");
    check_bucket_size(expected_bucket_size);
  }

  const int shelf_height = GetShelfWidget()->GetWindowBoundsInScreen().height();

  {
    SCOPED_TRACE("Upward drag with move - 1");
    generator->MoveTouchBy(0, -shelf_height / 2);
    ASSERT_TRUE(hotseat_moved());
    check_bucket_size(++expected_bucket_size);
  }

  {
    SCOPED_TRACE("Upward drag with move - 2");
    generator->MoveTouchBy(0, -shelf_height / 2);
    ASSERT_TRUE(hotseat_moved());
    check_bucket_size(++expected_bucket_size);
  }

  {
    SCOPED_TRACE("Downward drag with move");
    generator->MoveTouchBy(0, shelf_height / 2);
    ASSERT_TRUE(hotseat_moved());
    check_bucket_size(++expected_bucket_size);
  }

  {
    SCOPED_TRACE("Upward drag with move - 3");
    generator->MoveTouchBy(0, -shelf_height / 2);
    ASSERT_TRUE(hotseat_moved());
    check_bucket_size(++expected_bucket_size);
  }

  const int hotseat_height =
      GetPrimaryShelf()->hotseat_widget()->GetWindowBoundsInScreen().height();
  {
    SCOPED_TRACE("Upward drag with move above shelf - 1");
    generator->MoveTouchBy(0, -hotseat_height / 2);
    ASSERT_TRUE(hotseat_moved());
    check_bucket_size(++expected_bucket_size);
  }

  {
    SCOPED_TRACE("Upward drag with move above shelf - 2");
    generator->MoveTouchBy(0, -hotseat_height / 2);
    ASSERT_TRUE(hotseat_moved());
    check_bucket_size(++expected_bucket_size);
  }

  {
    SCOPED_TRACE("Downward drag with move above shelf");
    generator->MoveTouchBy(0, hotseat_height / 2);
    ASSERT_TRUE(hotseat_moved());
    check_bucket_size(++expected_bucket_size);
  }

  {
    SCOPED_TRACE("Upward drag with move above shelf - 3");
    generator->MoveTouchBy(0, -hotseat_height - hotseat_height / 2);
    ASSERT_TRUE(hotseat_moved());
    check_bucket_size(++expected_bucket_size);
  }

  // Once the hotseat has been fully extended, presentation time metric should
  // stop being reported, as the hotseat is expected to stop moving.
  {
    SCOPED_TRACE("Upward drag without moving - 1");
    generator->MoveTouchBy(0, -hotseat_height);
    ASSERT_FALSE(hotseat_moved());
    check_bucket_size(expected_bucket_size);
  }

  {
    SCOPED_TRACE("Upward drag without moving - 2");
    generator->MoveTouchBy(0, -hotseat_height / 2);
    ASSERT_FALSE(hotseat_moved());
    check_bucket_size(expected_bucket_size);
  }

  // Move hotseat downwards - the presentation time should not get reported
  // until the hotseat starts moving downwards.
  {
    SCOPED_TRACE("Downward drag without moving - 1");
    generator->MoveTouchBy(0, hotseat_height / 2);
    ASSERT_FALSE(hotseat_moved());
    check_bucket_size(expected_bucket_size);
  }

  {
    SCOPED_TRACE("Downward drag without moving - 2");
    generator->MoveTouchBy(0, hotseat_height);
    ASSERT_FALSE(hotseat_moved());
    check_bucket_size(expected_bucket_size);
  }

  generator->ReleaseTouch();

  window.reset();

  {
    SCOPED_TRACE("Drag ended.");
    histogram_tester.ExpectTotalCount(
        "Ash.HotseatTransition.Drag.PresentationTime", expected_bucket_size);
    histogram_tester.ExpectTotalCount(
        "Ash.HotseatTransition.Drag.PresentationTime.MaxLatency", 1);
  }
}

// TODO(manucornet): Enable this test once the new API for layer animation
// sequence observers is available.
TEST_P(HotseatWidgetTest, DISABLED_OverviewToHomeAnimationAndBackIsSmooth) {
  // Go into tablet mode and make sure animations are over.
  HotseatWidget* hotseat = GetPrimaryShelf()->hotseat_widget();
  {
    views::WidgetAnimationWaiter waiter(hotseat);
    TabletModeControllerTestApi().EnterTabletMode();
    waiter.WaitForAnimation();
  }

  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Go into overview and back to know what to expect in terms of bounds.
  const gfx::Rect shown_hotseat_bounds = hotseat->GetWindowBoundsInScreen();
  {
    views::WidgetAnimationWaiter waiter(hotseat);
    StartOverview();
    waiter.WaitForAnimation();
  }

  const gfx::Rect extended_hotseat_bounds = hotseat->GetWindowBoundsInScreen();
  {
    views::WidgetAnimationWaiter waiter(hotseat);
    EndOverview();
    waiter.WaitForAnimation();
  }

  // The extended hotseat should be higher (lower value of Y) than the
  // shown hotseat.
  EXPECT_GT(shown_hotseat_bounds.y(), extended_hotseat_bounds.y());

  // We should start with the hotseat in its shown position again.
  EXPECT_EQ(shown_hotseat_bounds, hotseat->GetWindowBoundsInScreen());

  {
    WidgetAnimationSmoothnessInspector inspector(hotseat);
    views::WidgetAnimationWaiter waiter(hotseat);
    StartOverview();
    waiter.WaitForAnimation();
    EXPECT_TRUE(inspector.CheckAnimation(4));
  }

  // The hotseat should now be extended.
  EXPECT_EQ(extended_hotseat_bounds, hotseat->GetWindowBoundsInScreen());

  {
    WidgetAnimationSmoothnessInspector inspector(hotseat);
    views::WidgetAnimationWaiter waiter(hotseat);
    EndOverview();
    waiter.WaitForAnimation();
    EXPECT_TRUE(inspector.CheckAnimation(4));
  }

  // And we should now be back where we started.
  EXPECT_EQ(shown_hotseat_bounds, hotseat->GetWindowBoundsInScreen());
}

class HotseatWidgetRTLTest : public ShelfLayoutManagerTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  // Use MOCK_TIME to increase number of commits during animation.
  HotseatWidgetRTLTest()
      : ShelfLayoutManagerTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_locale_(GetParam() ? "ar" : "") {}
  HotseatWidgetRTLTest(const HotseatWidgetRTLTest&) = delete;
  HotseatWidgetRTLTest& operator=(const HotseatWidgetRTLTest&) = delete;
  ~HotseatWidgetRTLTest() override = default;

 private:
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;
};

INSTANTIATE_TEST_SUITE_P(RTL, HotseatWidgetRTLTest, testing::Bool());

// The test to verify the hotseat transition animation from the extended state
// to the home launcher state.
// TODO(crbug.com/40182469): Disable this test due to flakiness.
TEST_P(HotseatWidgetRTLTest,
       DISABLED_VerifyTransitionFromExtendedModeToHomeLauncher) {
  TabletModeControllerTestApi().EnterTabletMode();
  const auto app_id =
      ShelfTestUtil::AddAppShortcut("fake_app", TYPE_PINNED_APP);

  // Open a window so the hotseat transitions to hidden state.
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  // Swipe the hotseat up to enter the extended mode.
  SwipeUpOnShelf();
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  // Animation should be long enough in order to collect sufficient data.
  // TODO(crbug.com/40181827): remove this line when we solve that issue.
  ui::ScopedAnimationDurationScaleMode animation_duration(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Wait until shelf animation completes.
  auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
  ShelfViewTestAPI shelf_test_api(shelf_view);
  shelf_test_api.RunMessageLoopUntilAnimationsDone();

  // Observe a shelf icon.
  auto* observed_view = shelf_view->GetShelfAppButton(app_id.id);
  LayerAnimationVerifier verifier(
      GetPrimaryShelf()->hotseat_widget()->GetNativeView()->layer(),
      observed_view);

  // Transit the hotseat from the extended state to the home launcher state.
  // Wait until the transition animation finishes.
  views::WidgetAnimationWaiter waiter(GetPrimaryShelf()->hotseat_widget());
  FlingUpOnShelf();
  waiter.WaitForAnimation();

  // Verify the hotseat state at the end of the animation.
  EXPECT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());
}

}  // namespace ash
