// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_ACTION_CONTEXT_MENU_H_
#define ASH_WM_DESKS_DESK_ACTION_CONTEXT_MENU_H_

#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"

namespace views {
class MenuRunner;
}  // namespace views

namespace ash {

// A context menu controller that generates a context menu for a DeskMiniView
// with an option to combine desks and an option to close a desk and all of its
// windows.
class DeskActionContextMenu : public views::ContextMenuController,
                              public ui::SimpleMenuModel::Delegate {
 public:
  // An enum with identifiers to link context menu items to their associated
  // functions.
  enum CommandId {
    // Closes target desk and moves its windows to another desk.
    kCombineDesks,
    // Saves target desk in DesksController and gives user option to undo the
    // desk before the desk is fully removed and its windows are closed.
    kCloseAll,
  };

  DeskActionContextMenu(const std::u16string& initial_combine_desks_target_name,
                        base::RepeatingClosure combine_desks_callback,
                        base::RepeatingClosure close_all_callback,
                        base::RepeatingClosure on_context_menu_closed_callback);
  DeskActionContextMenu(const DeskActionContextMenu&) = delete;
  DeskActionContextMenu& operator=(const DeskActionContextMenu&) = delete;
  ~DeskActionContextMenu() override;

  // Because the desk that we move the windows to in the combine desks operation
  // can change (such as when the user reorders desks), we need to update
  // `combine_desks_target_name_` before we show the context menu.
  void UpdateCombineDesksTargetName(
      const std::u16string& new_combine_desks_target_name);

  // Changes the visibility of the combine desks context menu item so that it
  // can reflect whether there are windows on the desk.
  void SetCombineDesksMenuItemVisibility(bool visible);

  // Closes the context menu if one is running.
  void MaybeCloseMenu();

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  void MenuClosed(ui::SimpleMenuModel* menu) override;

 private:
  friend class DesksTestApi;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // Callbacks to run when the combine desks option is selected, when the close
  // desk and windows option is selected, and when the menu is closed.
  base::RepeatingClosure combine_desks_callback_;
  base::RepeatingClosure close_all_callback_;
  base::RepeatingClosure on_context_menu_closed_callback_;

  ui::SimpleMenuModel context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
};

}  // namespace ash

#endif