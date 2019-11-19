// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_MENU_MODEL_ADAPTER_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_MENU_MODEL_ADAPTER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_menu/app_menu_model_adapter.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/menu/menu_types.h"

namespace ash {

// A class wrapping menu operations for apps in AppListView. Responsible for
// building, running, and recording histograms.
class APP_LIST_EXPORT AppListMenuModelAdapter
    : public ash::AppMenuModelAdapter {
 public:
  // The kinds of apps which show menus. This enum is used to record
  // metrics, if a new value is added make sure to modify
  // RecordHistogramOnMenuClosed().
  enum AppListViewAppType {
    FULLSCREEN_SEARCH_RESULT,
    FULLSCREEN_SUGGESTED,
    FULLSCREEN_APP_GRID,
    PEEKING_SUGGESTED,
    HALF_SEARCH_RESULT,
    SEARCH_RESULT,
    APP_LIST_APP_TYPE_LAST
  };

  AppListMenuModelAdapter(const std::string& app_id,
                          std::unique_ptr<ui::SimpleMenuModel> menu_model,
                          views::Widget* widget_owner,
                          ui::MenuSourceType source_type,
                          const AppLaunchedMetricParams& metric_params,
                          AppListViewAppType type,
                          base::OnceClosure on_menu_closed_callback,
                          bool is_tablet_mode);
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

  DISALLOW_COPY_AND_ASSIGN(AppListMenuModelAdapter);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_MENU_MODEL_ADAPTER_H_
