// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_presenter_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

class AppListPresenterImplTest : public AshTestBase {
 public:
  AppListPresenterImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  AppListPresenterImplTest(const AppListPresenterImplTest&) = delete;
  AppListPresenterImplTest& operator=(const AppListPresenterImplTest&) = delete;

  ~AppListPresenterImplTest() override = default;

  AppListPresenterImpl* presenter() {
    return Shell::Get()->app_list_controller()->fullscreen_presenter();
  }

  // Enables tablet mode and fast-forwards mock time until window occlusion
  // tracking is enabled. See TabletModeController::SuspendOcclusionTracker().
  // This is necessary for AppListPresenterImpl::IsAtLeastPartiallyVisible() to
  // return correct results.
  void EnableTabletMode() {
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    auto* occlusion_tracker =
        aura::Env::GetInstance()->GetWindowOcclusionTracker();
    while (occlusion_tracker->IsPaused()) {
      task_environment()->FastForwardBy(base::Milliseconds(100));
    }
  }

  // Shows the app list on the primary display.
  void ShowAppList() {
    presenter()->Show(AppListViewState::kFullscreenAllApps,
                      GetPrimaryDisplay().id(), base::TimeTicks(),
                      /*show_source=*/std::nullopt);
  }

  // Shows the Assistant UI.
  void ShowAssistantUI() {
    presenter()->ShowEmbeddedAssistantUI(/*show=*/true);
  }

  bool IsShowingAssistantUI() {
    return presenter()->IsShowingEmbeddedAssistantUI();
  }
};

// Tests, in tablet mode, that when specific container id widgets are focused,
// that the shelf background type remains in kHomeLauncher and does not change
// to kInApp.
TEST_F(AppListPresenterImplTest,
       ShelfBackgroundWithHomeLauncherAndContainerIdsShown) {
  // Enter tablet mode to display the home launcher.
  EnableTabletMode();
  ASSERT_TRUE(presenter()->GetTargetVisibility());
  ASSERT_TRUE(presenter()->IsAtLeastPartiallyVisible());
  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();
  EXPECT_EQ(ShelfBackgroundType::kHomeLauncher,
            shelf_layout_manager->shelf_background_type());
  HotseatWidget* hotseat = GetPrimaryShelf()->hotseat_widget();

  for (int id : AppListPresenterImpl::kIdsOfContainersThatWontHideAppList) {
    // Create a widget with a specific container id and make sure that the
    // kHomeLauncher background is still shown.
    std::unique_ptr<views::Widget> widget = CreateTestWidget(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr, id);

    EXPECT_EQ(ShelfBackgroundType::kHomeLauncher,
              shelf_layout_manager->shelf_background_type())
        << " container " << id;
    EXPECT_EQ(hotseat->state(), HotseatState::kShownHomeLauncher);
  }
}

// Tests that Assistant UI in tablet mode is closed when open another window.
TEST_F(AppListPresenterImplTest, HideAssistantUIOnFocusOut) {
  // Enter tablet mode to display the home launcher.
  EnableTabletMode();
  EXPECT_TRUE(presenter()->IsVisibleDeprecated());
  EXPECT_FALSE(IsShowingAssistantUI());

  // Open a window to cover Home Launcher.
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  EXPECT_FALSE(presenter()->IsVisibleDeprecated());

  // Open Assistant UI.
  ShowAssistantUI();
  // Assistant UI is visible but Home Launcher is considered not visible.
  EXPECT_TRUE(IsShowingAssistantUI());
  EXPECT_FALSE(presenter()->IsVisibleDeprecated());

  // Open another window should close Assistant UI.
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  EXPECT_FALSE(IsShowingAssistantUI());
  EXPECT_FALSE(presenter()->IsVisibleDeprecated());
}

// Regression test for https://crbug.com/1235056
// Tests that shelf observers are cleared when shelf is destroyed.
TEST_F(AppListPresenterImplTest, ClearShelfObserversOnShelfRemoval) {
  // Set up multidisplay, and open the app list on secondary monitor, so app
  // list presenter starts observing the shelf state.
  UpdateDisplay("600x400,600x400");

  // Enter tablet mode, so the test can trigger tablet mode exit later on.
  EnableTabletMode();

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Remove the secondary display, and exit tablet mode to trigger app list view
  // dismissal. Note that the display will be removed before the app list close
  // animation completes.
  UpdateDisplay("600x400");

  base::RunLoop run_loop;
  Shell::Get()
      ->app_list_controller()
      ->SetStateTransitionAnimationCallbackForTesting(
          base::BindLambdaForTesting([&](AppListViewState state) {
            if (state == AppListViewState::kClosed)
              run_loop.Quit();
          }));
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  run_loop.Run();

  // Just verify there was no crash.
}

}  // namespace
}  // namespace ash
