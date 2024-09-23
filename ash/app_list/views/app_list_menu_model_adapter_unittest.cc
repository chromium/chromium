// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_menu_model_adapter.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_types.h"

namespace ash {
namespace {

class AppListMenuModelAdapterTest
    : public AshTestBase,
      public testing::WithParamInterface</*is_tablet_mode=*/bool> {
 public:
  std::unique_ptr<AppListMenuModelAdapter> CreateAdapter(
      const AppLaunchedMetricParams& metric_params,
      AppListMenuModelAdapter::AppListViewAppType type,
      bool is_tablet_mode) const {
    auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
    menu_model->AddItem(LAUNCH_NEW, u"Launch app");
    menu_model->AddItem(NOTIFICATION_CONTAINER, u"Test menu item");

    return std::make_unique<AppListMenuModelAdapter>(
        "test-app-id", std::move(menu_model), nullptr,
        ui::MenuSourceType::MENU_SOURCE_MOUSE, metric_params, type,
        base::OnceClosure(), is_tablet_mode, AppCollection::kUnknown);
  }

  std::string AppendClamshellOrTabletModePostfix(
      const std::string& histogram_name) const {
    return base::JoinString(
        {histogram_name, is_tablet_mode() ? "TabletMode" : "ClamshellMode"},
        ".");
  }

  bool is_tablet_mode() const { return GetParam(); }
};

// Tests that NOTIFICATION_CONTAINER is enabled. This ensures that the
// container is able to handle gesture events.
TEST_F(AppListMenuModelAdapterTest, NotificationContainerEnabled) {
  auto adapter = CreateAdapter(
      AppLaunchedMetricParams(),
      AppListMenuModelAdapter::PRODUCTIVITY_LAUNCHER_APP_GRID, false);
  EXPECT_TRUE(adapter->IsCommandEnabled(NOTIFICATION_CONTAINER));
}

TEST_P(AppListMenuModelAdapterTest, RecordsHistogramOnMenuClosed) {
  const struct {
    AppListMenuModelAdapter::AppListViewAppType type;
    std::string base_histogram_name;
    bool has_non_tablet_clamshell_histograms;
  } test_cases[] = {
      {AppListMenuModelAdapter::PRODUCTIVITY_LAUNCHER_RECENT_APP,
       "ProductivityLauncherRecentApp", false},
      {AppListMenuModelAdapter::PRODUCTIVITY_LAUNCHER_APP_GRID,
       "ProductivityLauncherAppGrid", false},
      {AppListMenuModelAdapter::PRODUCTIVITY_LAUNCHER_APPS_COLLECTIONS,
       "AppsCollections", false},
  };

  for (const auto& test_case : test_cases) {
    const base::HistogramTester histogram_tester;

    const auto adapter = CreateAdapter(AppLaunchedMetricParams(),
                                       test_case.type, is_tablet_mode());
    adapter->Run(gfx::Rect(), views::MenuAnchorPosition::kBottomCenter, 0);
    adapter->Cancel();

    const auto show_source_histogram_name = base::JoinString(
        {"Apps", "ContextMenuShowSourceV2", test_case.base_histogram_name},
        ".");
    const auto user_journey_time_histogram_name = base::JoinString(
        {"Apps", "ContextMenuUserJourneyTimeV2", test_case.base_histogram_name},
        ".");

    if (test_case.has_non_tablet_clamshell_histograms) {
      histogram_tester.ExpectUniqueSample(
          show_source_histogram_name, ui::MenuSourceType::MENU_SOURCE_MOUSE, 1);
      histogram_tester.ExpectTotalCount(user_journey_time_histogram_name, 1);
    }
    histogram_tester.ExpectUniqueSample(
        AppendClamshellOrTabletModePostfix(show_source_histogram_name),
        ui::MenuSourceType::MENU_SOURCE_MOUSE, 1);
    histogram_tester.ExpectTotalCount(
        AppendClamshellOrTabletModePostfix(user_journey_time_histogram_name),
        1);
  }
}

TEST_P(AppListMenuModelAdapterTest, RecordsAppLaunched) {
  const struct {
    AppListLaunchedFrom launched_from;
    AppListUserAction expected_user_action;
  } test_cases[] = {
      {AppListLaunchedFrom::kLaunchedFromGrid,
       AppListUserAction::kAppLaunchFromAppsGrid},
      {AppListLaunchedFrom::kLaunchedFromRecentApps,
       AppListUserAction::kAppLaunchFromRecentApps},
      {AppListLaunchedFrom::kLaunchedFromSearchBox,
       AppListUserAction::kOpenAppSearchResult},
      {AppListLaunchedFrom::kLaunchedFromAppsCollections,
       AppListUserAction::kAppLauncherFromAppsCollections},
  };

  for (const auto& test_case : test_cases) {
    const base::HistogramTester histogram_tester;

    AppLaunchedMetricParams metric_params;
    metric_params.launched_from = test_case.launched_from;
    metric_params.launch_type = AppListLaunchType::kApp;
    metric_params.is_tablet_mode = is_tablet_mode();
    metric_params.app_list_view_state = AppListViewState::kFullscreenAllApps;

    const auto adapter = CreateAdapter(
        metric_params, AppListMenuModelAdapter::PRODUCTIVITY_LAUNCHER_APP_GRID,
        is_tablet_mode());
    adapter->Run(gfx::Rect(), views::MenuAnchorPosition::kBottomCenter, 0);
    adapter->ExecuteCommand(LAUNCH_NEW, /*mouse_event_flags=*/0);

    histogram_tester.ExpectUniqueSample("Apps.AppListAppLaunchedV2",
                                        metric_params.launched_from, 1);
    histogram_tester.ExpectUniqueSample(
        AppendClamshellOrTabletModePostfix("Apps.AppList.UserAction"),
        test_case.expected_user_action, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         AppListMenuModelAdapterTest,
                         /*is_tablet_mode=*/testing::Bool());

}  // namespace
}  // namespace ash
