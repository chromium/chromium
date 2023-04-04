// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_menu_model_adapter.h"

#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/metrics/histogram_macros.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

AppListMenuModelAdapter::AppListMenuModelAdapter(
    const std::string& app_id,
    std::unique_ptr<ui::SimpleMenuModel> menu_model,
    views::Widget* widget_owner,
    ui::MenuSourceType source_type,
    const AppLaunchedMetricParams& metric_params,
    AppListViewAppType type,
    base::OnceClosure on_menu_closed_callback,
    bool is_tablet_mode)
    : AppMenuModelAdapter(app_id,
                          std::move(menu_model),
                          widget_owner,
                          source_type,
                          std::move(on_menu_closed_callback),
                          is_tablet_mode),
      metric_params_(metric_params),
      type_(type) {
  DCHECK_NE(AppListViewAppType::APP_LIST_APP_TYPE_LAST, type);
}

AppListMenuModelAdapter::~AppListMenuModelAdapter() = default;

void AppListMenuModelAdapter::RecordHistogramOnMenuClosed() {
  const base::TimeDelta user_journey_time =
      base::TimeTicks::Now() - menu_open_time();
  switch (type_) {
    case FULLSCREEN_SUGGESTED:
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.ContextMenuShowSourceV2.SuggestedAppFullscreen", source_type(),
          ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
      UMA_HISTOGRAM_TIMES(
          "Apps.ContextMenuUserJourneyTimeV2.SuggestedAppFullscreen",
          user_journey_time);
      if (is_tablet_mode()) {
        UMA_HISTOGRAM_ENUMERATION(
            "Apps.ContextMenuShowSourceV2.SuggestedAppFullscreen.TabletMode",
            source_type(), ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
        UMA_HISTOGRAM_TIMES(
            "Apps.ContextMenuUserJourneyTimeV2.SuggestedAppFullscreen."
            "TabletMode",
            user_journey_time);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            "Apps.ContextMenuShowSourceV2.SuggestedAppFullscreen.ClamshellMode",
            source_type(), ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
        UMA_HISTOGRAM_TIMES(
            "Apps.ContextMenuUserJourneyTimeV2.SuggestedAppFullscreen."
            "ClamshellMode",
            user_journey_time);
      }
      break;
    case FULLSCREEN_APP_GRID:
      UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSourceV2.AppGrid",
                                source_type(), ui::MENU_SOURCE_TYPE_LAST);
      UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTimeV2.AppGrid",
                          user_journey_time);
      if (is_tablet_mode()) {
        UMA_HISTOGRAM_ENUMERATION(
            "Apps.ContextMenuShowSourceV2.AppGrid.TabletMode", source_type(),
            ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
        UMA_HISTOGRAM_TIMES(
            "Apps.ContextMenuUserJourneyTimeV2.AppGrid.TabletMode",
            user_journey_time);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            "Apps.ContextMenuShowSourceV2.AppGrid.ClamshellMode", source_type(),
            ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
        UMA_HISTOGRAM_TIMES(
            "Apps.ContextMenuUserJourneyTimeV2.AppGrid.ClamshellMode",
            user_journey_time);
      }
      break;
    case PRODUCTIVITY_LAUNCHER_RECENT_APP:
      if (is_tablet_mode()) {
        UMA_HISTOGRAM_ENUMERATION(
            "Apps.ContextMenuShowSourceV2.ProductivityLauncherRecentApp."
            "TabletMode",
            source_type(), ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
        UMA_HISTOGRAM_TIMES(
            "Apps.ContextMenuUserJourneyTimeV2.ProductivityLauncherRecentApp."
            "TabletMode",
            user_journey_time);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            "Apps.ContextMenuShowSourceV2.ProductivityLauncherRecentApp."
            "ClamshellMode",
            source_type(), ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
        UMA_HISTOGRAM_TIMES(
            "Apps.ContextMenuUserJourneyTimeV2.ProductivityLauncherRecentApp."
            "ClamshellMode",
            user_journey_time);
      }
      break;
    case PRODUCTIVITY_LAUNCHER_APP_GRID:
      if (is_tablet_mode()) {
        UMA_HISTOGRAM_ENUMERATION(
            "Apps.ContextMenuShowSourceV2.ProductivityLauncherAppGrid."
            "TabletMode",
            source_type(), ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
        UMA_HISTOGRAM_TIMES(
            "Apps.ContextMenuUserJourneyTimeV2.ProductivityLauncherAppGrid."
            "TabletMode",
            user_journey_time);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            "Apps.ContextMenuShowSourceV2.ProductivityLauncherAppGrid."
            "ClamshellMode",
            source_type(), ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
        UMA_HISTOGRAM_TIMES(
            "Apps.ContextMenuUserJourneyTimeV2.ProductivityLauncherAppGrid."
            "ClamshellMode",
            user_journey_time);
      }
      break;
    case FULLSCREEN_SEARCH_RESULT:
      UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSourceV2.SearchResult",
                                source_type(),
                                ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
      UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTimeV2.SearchResult",
                          user_journey_time);
      if (is_tablet_mode()) {
        UMA_HISTOGRAM_ENUMERATION(
            "Apps.ContextMenuShowSourceV2.SearchResult.TabletMode",
            source_type(), ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
        UMA_HISTOGRAM_TIMES(
            "Apps.ContextMenuUserJourneyTimeV2.SearchResult.TabletMode",
            user_journey_time);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            "Apps.ContextMenuShowSourceV2.SearchResult.ClamshellMode",
            source_type(), ui::MenuSourceType::MENU_SOURCE_TYPE_LAST);
        UMA_HISTOGRAM_TIMES(
            "Apps.ContextMenuUserJourneyTimeV2.SearchResult.ClamshellMode",
            user_journey_time);
      }
      break;
    case APP_LIST_APP_TYPE_LAST:
      NOTREACHED();
      break;
  }
}

bool AppListMenuModelAdapter::IsCommandEnabled(int id) const {
  // NOTIFICATION_CONTAINER is always enabled. It is added to this model by
  // NotificationMenuController. It is not known by model()'s delegate (i.e.
  // an instance of AppContextMenu). Check for it first.
  if (id == NOTIFICATION_CONTAINER)
    return true;

  return AppMenuModelAdapter::IsCommandEnabled(id);
}

void AppListMenuModelAdapter::ExecuteCommand(int id, int mouse_event_flags) {
  MaybeRecordAppLaunched(id);

  // Note that ExecuteCommand might delete us.
  AppMenuModelAdapter::ExecuteCommand(id, mouse_event_flags);
}

void AppListMenuModelAdapter::MaybeRecordAppLaunched(int command_id) {
  // Early out if |command_id| is not considered as app launch.
  if (!IsCommandIdAnAppLaunch(command_id))
    return;

  switch (metric_params_.launch_type) {
    case AppListLaunchType::kSearchResult:
      break;
    case AppListLaunchType::kAppSearchResult:
    case AppListLaunchType::kApp:
      RecordAppListAppLaunched(
          metric_params_.launched_from, metric_params_.app_list_view_state,
          metric_params_.is_tablet_mode, metric_params_.app_list_shown);

      switch (metric_params_.launched_from) {
        case AppListLaunchedFrom::kLaunchedFromGrid:
          RecordLauncherWorkflowMetrics(
              AppListUserAction::kAppLaunchFromAppsGrid,
              metric_params_.is_tablet_mode,
              metric_params_.launcher_show_timestamp);
          break;
        case AppListLaunchedFrom::kLaunchedFromRecentApps:
          RecordLauncherWorkflowMetrics(
              AppListUserAction::kAppLaunchFromRecentApps,
              metric_params_.is_tablet_mode,
              metric_params_.launcher_show_timestamp);
          break;
        case AppListLaunchedFrom::kLaunchedFromSearchBox:
          RecordLauncherWorkflowMetrics(AppListUserAction::kOpenAppSearchResult,
                                        metric_params_.is_tablet_mode,
                                        metric_params_.launcher_show_timestamp);
          break;
        case AppListLaunchedFrom::DEPRECATED_kLaunchedFromSuggestionChip:
        case AppListLaunchedFrom::kLaunchedFromContinueTask:
        case AppListLaunchedFrom::kLaunchedFromShelf:
        case AppListLaunchedFrom::kLaunchedFromQuickAppAccess:
          NOTREACHED();
          break;
      }
      break;
  }
}

}  // namespace ash
