// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/app_menu_model_adapter.h"

#include <memory>
#include <utility>

#include "ash/app_menu/notification_menu_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/metrics/histogram_functions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"

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
  std::unique_ptr<views::MenuItemView> root = CreateMenu();
  root_ = root.get();
  if (ash::features::IsNotificationsInContextMenuEnabled()) {
    notification_menu_controller_ =
        std::make_unique<NotificationMenuController>(app_id_, root_, this);
  }
  menu_runner_ =
      std::make_unique<views::MenuRunner>(std::move(root), run_types);
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

int AppMenuModelAdapter::GetCommandIdForHistograms(int command_id) {
  return command_id;
}

base::TimeTicks AppMenuModelAdapter::GetClosingEventTime() {
  DCHECK(menu_runner_);
  return menu_runner_->closing_event_time();
}

views::Widget* AppMenuModelAdapter::GetSubmenuWidget() {
  if (root_ && root_->GetSubmenu())
    return root_->GetSubmenu()->GetWidget();
  return nullptr;
}

void AppMenuModelAdapter::ExecuteCommand(int id, int mouse_event_flags) {
  // Note that the command execution may cause this to get deleted - for
  // example, for search result menus, the command could open an app window
  // causing the app list search to get cleared, destroying non-zero state
  // search results.
  auto weak_self = weak_ptr_factory_.GetWeakPtr();
  RecordExecuteCommandHistogram(GetCommandIdForHistograms(id));
  views::MenuModelAdapter::ExecuteCommand(id, mouse_event_flags);

  if (!weak_self)
    return;

  if (id >= USE_LAUNCH_TYPE_COMMAND_START &&
      id <= USE_LAUNCH_TYPE_COMMAND_END) {
    // Rebuild the menu to ensure that the `LAUNCH_NEW` menu item is refreshed
    // after changing the app launch type. Note: this closes the submenu with
    // `USE_LAUNCH_TYPE_*` commands.
    BuildMenu(root_);
  }
}

void AppMenuModelAdapter::OnMenuClosed(views::MenuItemView* menu) {
  DCHECK_NE(base::TimeTicks(), menu_open_time_);
  RecordHistogramOnMenuClosed();

  // No |widget_owner_| in tests.
  if (widget_owner_ && widget_owner_->GetRootView()) {
    widget_owner_->GetRootView()->NotifyAccessibilityEvent(
        ax::mojom::Event::kMenuEnd,
        /*send_native_event=*/true);
  }
  if (on_menu_closed_callback_)
    std::move(on_menu_closed_callback_).Run();
}

bool AppMenuModelAdapter::ShouldExecuteCommandWithoutClosingMenu(
    int id,
    const ui::Event& event) {
  return id >= USE_LAUNCH_TYPE_COMMAND_START &&
         id <= USE_LAUNCH_TYPE_COMMAND_END;
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
