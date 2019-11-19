// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_MENU_APP_MENU_MODEL_ADAPTER_H_
#define ASH_APP_MENU_APP_MENU_MODEL_ADAPTER_H_

#include <memory>
#include <string>

#include "ash/app_menu/app_menu_export.h"
#include "base/callback.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_types.h"

namespace ui {
class SimpleMenuModel;
}

namespace views {
class MenuItemView;
class MenuRunner;
class Widget;
}  // namespace views

namespace ash {

class NotificationMenuController;

class APP_MENU_EXPORT AppMenuModelAdapter : public views::MenuModelAdapter {
 public:
  AppMenuModelAdapter(const std::string& app_id,
                      std::unique_ptr<ui::SimpleMenuModel> model,
                      views::Widget* widget_owner,
                      ui::MenuSourceType source_type,
                      base::OnceClosure on_menu_closed_callback,
                      bool is_tablet_mode);
  ~AppMenuModelAdapter() override;

  // Builds the view tree and shows the menu.
  void Run(const gfx::Rect& menu_anchor_rect,
           views::MenuAnchorPosition menu_anchor_position,
           int run_types);
  // Whether this is showing a menu.
  bool IsShowingMenu() const;

  // Closes the menu if one is being shown.
  void Cancel();

  base::TimeTicks GetClosingEventTime();

  // Overridden from views::MenuModelAdapter:
  void ExecuteCommand(int id, int mouse_event_flags) override;
  void OnMenuClosed(views::MenuItemView* menu) override;

  ui::SimpleMenuModel* model() { return model_.get(); }

 protected:
  const std::string& app_id() const { return app_id_; }
  base::TimeTicks menu_open_time() const { return menu_open_time_; }
  ui::MenuSourceType source_type() const { return source_type_; }
  bool is_tablet_mode() const { return is_tablet_mode_; }

  // Helper method to record ExecuteCommand() histograms.
  void RecordExecuteCommandHistogram(int command_id);

  // Records histograms on menu closed, such as the user journey time and show
  // source histograms.
  virtual void RecordHistogramOnMenuClosed() = 0;

 private:
  // The application identifier used to fetch active notifications to display.
  const std::string app_id_;

  // The list of items which will be shown in the menu.
  std::unique_ptr<ui::SimpleMenuModel> model_;

  // Responsible for adding the container MenuItemView to the parent
  // MenuItemView, and adding NOTIFICATION_CONTAINER to the model.
  std::unique_ptr<NotificationMenuController> notification_menu_controller_;

  // The parent widget of the context menu.
  views::Widget* widget_owner_;

  // The event type which was used to show the menu.
  const ui::MenuSourceType source_type_;

  // The callback which is triggered when the menu is closed.
  base::OnceClosure on_menu_closed_callback_;

  // The root MenuItemView which contains all child MenuItemViews. Owned by
  // |menu_runner_|.
  views::MenuItemView* root_ = nullptr;

  // Used to show the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The timestamp taken when the menu is opened. Used in metrics.
  base::TimeTicks menu_open_time_;

  // Whether tablet mode is active.
  bool is_tablet_mode_;

  DISALLOW_COPY_AND_ASSIGN(AppMenuModelAdapter);
};

}  // namespace ash

#endif  // ASH_APP_MENU_APP_MENU_MODEL_ADAPTER_H_
