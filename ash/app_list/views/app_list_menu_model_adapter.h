// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_MENU_MODEL_ADAPTER_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_MENU_MODEL_ADAPTER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_types.h"

namespace ash {

// A class wrapping menu operations for apps in AppListView. Responsible for
// building, running, and recording histograms.
class ASH_EXPORT AppListMenuModelAdapter : public AppMenuModelAdapter {
 public:
  // The kinds of apps which show menus. This enum is used to record
  // metrics, if a new value is added make sure to modify
  // RecordHistogramOnMenuClosed().
  enum AppListViewAppType {
    // Usage removed.
    // FULLSCREEN_SEARCH_RESULT = 0,
    // FULLSCREEN_SUGGESTED = 1,
    // FULLSCREEN_APP_GRID = 2,
    // PEEKING_SUGGESTED = 3,
    // HALF_SEARCH_RESULT = 4,
    // SEARCH_RESULT = 5,

    PRODUCTIVITY_LAUNCHER_RECENT_APP = 6,
    PRODUCTIVITY_LAUNCHER_APP_GRID = 7,
    PRODUCTIVITY_LAUNCHER_APPS_COLLECTIONS = 8,
    APP_LIST_APP_TYPE_LAST = 9
  };

  AppListMenuModelAdapter(const std::string& app_id,
                          std::unique_ptr<ui::SimpleMenuModel> menu_model,
                          views::Widget* widget_owner,
                          ui::MenuSourceType source_type,
                          const AppLaunchedMetricParams& metric_params,
                          AppListViewAppType type,
                          base::OnceClosure on_menu_closed_callback,
                          bool is_tablet_mode,
                          AppCollection collection);

  AppListMenuModelAdapter(const AppListMenuModelAdapter&) = delete;
  AppListMenuModelAdapter& operator=(const AppListMenuModelAdapter&) = delete;

  ~AppListMenuModelAdapter() override;

  // Overridden from AppMenuModelAdapter:
  void RecordHistogramOnMenuClosed() override;

  // Overridden from views::MenuModelAdapter:
  bool IsCommandEnabled(int id) const override;
  void ExecuteCommand(int id, int mouse_event_flags) override;

 private:
  // Calls RecordAppListAppLaunched() to record app launch related metrics if
  // conditions are met.
  void MaybeRecordAppLaunched(int command_id);

  const AppLaunchedMetricParams metric_params_;

  // The type of app which is using this object to show a menu.
  const AppListViewAppType type_;

  const AppCollection collection_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_MENU_MODEL_ADAPTER_H_
