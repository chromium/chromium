// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_nudge_controller.h"

#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "ui/display/screen.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

bool IsTabletMode() {
  return display::Screen::GetScreen()->InTabletMode();
}

// Returns the number of times the nudge has been shown. Note that the count
// will be updated only when the nudge becomes invisible.
int GetReorderNudgeShownCount() {
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  return AppListNudgeController::GetShownCount(
      pref_service, AppListNudgeController::NudgeType::kReorderNudge);
}

}  // namespace

class AppListNudgeControllerTest : public AshTestBase {
 public:
  AppListNudgeControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  AppListNudgeControllerTest(const AppListNudgeControllerTest&) = delete;
  AppListNudgeControllerTest& operator=(const AppListNudgeControllerTest&) =
      delete;
  ~AppListNudgeControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    GetAppListTestHelper()->DisableAppListNudge(false);
  }

  AppListNudgeController* GetNudgeController() {
    if (!IsTabletMode()) {
      return GetAppListTestHelper()
          ->GetBubbleAppsPage()
          ->app_list_nudge_controller();
    }

    return GetAppListTestHelper()
        ->GetAppsContainerView()
        ->app_list_nudge_controller();
  }

  AppListToastContainerView* GetToastContainerView() {
    if (!IsTabletMode()) {
      return GetAppListTestHelper()
          ->GetBubbleAppsPage()
          ->toast_container_for_test();
    }

    return GetAppListTestHelper()->GetAppsContainerView()->toast_container();
  }

  // Show app list and wait long enough for the nudge to be considered shown.
  void ShowAppListAndWait() {
    Shell::Get()->app_list_controller()->ShowAppList(
        AppListShowSource::kSearchKey);
    task_environment()->AdvanceClock(base::Seconds(1));
  }

  void DismissAppList() { GetAppListTestHelper()->Dismiss(); }
};

TEST_F(AppListNudgeControllerTest, Basic) {
  // Simulate a user login.
  SimulateUserLogin("user@gmail.com");

  // The reorder nudge should show 3 times to the users.
  ShowAppListAndWait();
  EXPECT_TRUE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kReorderNudge,
            GetToastContainerView()->current_toast());
  DismissAppList();
  ShowAppListAndWait();
  EXPECT_TRUE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kReorderNudge,
            GetToastContainerView()->current_toast());
  DismissAppList();
  ShowAppListAndWait();
  EXPECT_TRUE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kReorderNudge,
            GetToastContainerView()->current_toast());
  DismissAppList();

  // After the fourth time opening the app list, the nudge should be removed.
  ShowAppListAndWait();
  EXPECT_FALSE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kNone, GetToastContainerView()->current_toast());
  DismissAppList();
}

TEST_F(AppListNudgeControllerTest, StopShowingNudgeAfterReordering) {
  // Simulate a user login.
  SimulateUserLogin("user@gmail.com");

  // The reorder nudge should show for the first time.
  ShowAppListAndWait();
  EXPECT_TRUE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kReorderNudge,
            GetToastContainerView()->current_toast());
  // Simulate that the app list is reordered by name.
  Shell::Get()->app_list_controller()->UpdateAppListWithNewTemporarySortOrder(
      AppListSortOrder::kNameAlphabetical, /*animate=*/false,
      base::OnceClosure());
  EXPECT_TRUE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kReorderUndo,
            GetToastContainerView()->current_toast());
  DismissAppList();

  // If the app list was reordered, remove the nudge from the app list when the
  // app list is opened next time.
  ShowAppListAndWait();
  EXPECT_EQ(GetNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kNone);
  DismissAppList();
}

TEST_F(AppListNudgeControllerTest, TabletModeVisibilityTest) {
  // Simulate a user login.
  SimulateUserLogin("user@gmail.com");

  ShowAppListAndWait();
  EXPECT_TRUE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kReorderNudge,
            GetToastContainerView()->current_toast());
  // Change to tablet mode. The bubble app list is hidden and fullscreen app
  // list is showing.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(1, GetReorderNudgeShownCount());
  EXPECT_TRUE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kReorderNudge,
            GetToastContainerView()->current_toast());
  // Wait for long enough for the nudge to be considered shown.
  task_environment()->AdvanceClock(base::Seconds(1));

  // Open a window to make the app list invisible. This will update the prefs in
  // nudge controller.
  std::unique_ptr<aura::Window> window = AshTestBase::CreateTestWindow();
  wm::ActivateWindow(window.get());
  EXPECT_EQ(2, GetReorderNudgeShownCount());
  // Close the window and return back to app list.
  window->Hide();
  EXPECT_TRUE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kReorderNudge,
            GetToastContainerView()->current_toast());
  // Wait for long enough for the nudge to be considered shown.
  task_environment()->AdvanceClock(base::Seconds(1));

  // Activate the search box. The nudge will become inactive but the nudge view
  // still exists.
  auto* search_box = GetAppListTestHelper()->GetSearchBoxView();
  search_box->SetSearchBoxActive(true, ui::EventType::kMousePressed);
  // For the case where the nudge is visible but inactive, the count doesn't
  // increment as the nudge is still visible.
  EXPECT_EQ(2, GetReorderNudgeShownCount());
  EXPECT_TRUE(GetToastContainerView()->IsToastVisible());

  // Exit the search view. The nudge should be visible and active now.
  search_box->SetSearchBoxActive(false, ui::EventType::kMousePressed);
  EXPECT_TRUE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kReorderNudge,
            GetToastContainerView()->current_toast());

  // Change to tablet mode. The nudge should be removed when the next time the
  // app list is shown.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(3, GetReorderNudgeShownCount());
  ShowAppListAndWait();
  EXPECT_FALSE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kNone, GetToastContainerView()->current_toast());
}

TEST_F(AppListNudgeControllerTest, ReorderNudgeDismissButton) {
  // Simulate a user login.
  SimulateUserLogin("user@gmail.com");

  ShowAppListAndWait();
  EXPECT_TRUE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kReorderNudge,
            GetToastContainerView()->current_toast());

  // Dismiss the reorder nudge and check that it is no longer visible.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(GetToastContainerView()
                                   ->GetToastButton()
                                   ->GetBoundsInScreen()
                                   .CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_FALSE(GetToastContainerView()->IsToastVisible());

  // Close and reopen app list to make sure that the reorder nudge is no longer
  // shown after being dismissed.
  DismissAppList();
  ShowAppListAndWait();
  EXPECT_FALSE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kNone, GetToastContainerView()->current_toast());
}

TEST_F(AppListNudgeControllerTest, ReorderUndoCloseButton) {
  // Simulate a user login.
  SimulateUserLogin("user@gmail.com");

  ShowAppListAndWait();

  // Simulate that the app list is reordered by name and check that the reorder
  // undo nudge is shown.
  Shell::Get()->app_list_controller()->UpdateAppListWithNewTemporarySortOrder(
      AppListSortOrder::kNameAlphabetical, /*animate=*/false,
      base::OnceClosure());
  EXPECT_TRUE(GetToastContainerView()->IsToastVisible());
  EXPECT_EQ(AppListToastType::kReorderUndo,
            GetToastContainerView()->current_toast());

  GetToastContainerView()->GetWidget()->LayoutRootViewIfNecessary();

  // Click the close button and check that the nudge is no longer visible.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(GetToastContainerView()
                                   ->GetCloseButton()
                                   ->GetBoundsInScreen()
                                   .CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_FALSE(GetToastContainerView()->IsToastVisible());
}

}  // namespace ash
