// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/app_menu_model_adapter.h"

#include "ash/app_menu/notification_menu_controller.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

namespace {

// The UMA histogram that logs the commands which are executed on non-app
// context menus.
constexpr char kNonAppContextMenuExecuteCommand[] =
    "Apps.ContextMenuExecuteCommand.NotFromApp";
constexpr char kNonAppContextMenuExecuteCommandInTablet[] =
    "Apps.ContextMenuExecuteCommand.NotFromApp.TabletMode";
constexpr char kNonAppContextMenuExecuteCommandInClamshell[] =
    "Apps.ContextMenuExecuteCommand.NotFromApp.ClamshellMode";

// The UMA histogram that logs the commands which are executed on app context
// menus.
constexpr char kAppContextMenuExecuteCommand[] =
    "Apps.ContextMenuExecuteCommand.FromApp";
constexpr char kAppContextMenuExecuteCommandInTablet[] =
    "Apps.ContextMenuExecuteCommand.FromApp.TabletMode";
constexpr char kAppContextMenuExecuteCommandInClamshell[] =
    "Apps.ContextMenuExecuteCommand.FromApp.ClamshellMode";

}  // namespace

AppMenuModelAdapter::AppMenuModelAdapter(
    const std::string& app_id,
    std::unique_ptr<ui::SimpleMenuModel> model,
    views::Widget* widget_owner,
    ui::MenuSourceType source_type,
    base::OnceClosure on_menu_closed_callback,
    bool is_tablet_mode)
    : views::MenuModelAdapter(model.get()),
      app_id_(app_id),
      model_(std::move(model)),
      widget_owner_(widget_owner),
      source_type_(source_type),
      on_menu_closed_callback_(std::move(on_menu_closed_callback)),
      is_tablet_mode_(is_tablet_mode) {}

AppMenuModelAdapter::~AppMenuModelAdapter() = default;

void AppMenuModelAdapter::Run(const gfx::Rect& menu_anchor_rect,
                              views::MenuAnchorPosition menu_anchor_position,
                              int run_types) {
  DCHECK(!root_);
  DCHECK(model_);

  menu_open_time_ = base::TimeTicks::Now();
  root_ = CreateMenu();
  if (features::IsNotificationIndicatorEnabled()) {
    notification_menu_controller_ =
        std::make_unique<NotificationMenuController>(app_id_, root_, this);
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(root_, run_types);
  menu_runner_->RunMenuAt(widget_owner_, nullptr /* MenuButtonController */,
                          menu_anchor_rect, menu_anchor_position, source_type_);
}

bool AppMenuModelAdapter::IsShowingMenu() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

void AppMenuModelAdapter::Cancel() {
  if (!IsShowingMenu())
    return;
  menu_runner_->Cancel();
}

base::TimeTicks AppMenuModelAdapter::GetClosingEventTime() {
  DCHECK(menu_runner_);
  return menu_runner_->closing_event_time();
}

void AppMenuModelAdapter::ExecuteCommand(int id, int mouse_event_flags) {
  views::MenuModelAdapter::ExecuteCommand(id, mouse_event_flags);
  RecordExecuteCommandHistogram(id);
}

void AppMenuModelAdapter::OnMenuClosed(views::MenuItemView* menu) {
  DCHECK_NE(base::TimeTicks(), menu_open_time_);
  RecordHistogramOnMenuClosed();

  if (on_menu_closed_callback_)
    std::move(on_menu_closed_callback_).Run();
}

void AppMenuModelAdapter::RecordExecuteCommandHistogram(int command_id) {
  const bool is_not_from_app = app_id().empty();
  base::UmaHistogramSparse(is_not_from_app ? kNonAppContextMenuExecuteCommand
                                           : kAppContextMenuExecuteCommand,
                           command_id);
  if (is_tablet_mode_) {
    base::UmaHistogramSparse(is_not_from_app
                                 ? kNonAppContextMenuExecuteCommandInTablet
                                 : kAppContextMenuExecuteCommandInTablet,
                             command_id);
  } else {
    base::UmaHistogramSparse(is_not_from_app
                                 ? kNonAppContextMenuExecuteCommandInClamshell
                                 : kAppContextMenuExecuteCommandInClamshell,
                             command_id);
  }
}

}  // namespace ash
