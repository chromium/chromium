// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_ACTION_CONTEXT_MENU_H_
#define ASH_WM_DESKS_DESK_ACTION_CONTEXT_MENU_H_

#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/menu/menu_types.h"

namespace views {
class MenuItemView;
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

namespace ash {

// A context menu controller that generates a context menu for a DeskMiniView
// with an option to combine desks and an option to close a desk and all of its
// windows.
class DeskActionContextMenu : public views::ContextMenuController,
                              public ui::SimpleMenuModel::Delegate {
 public:
  // Config for the menu. Determines what the context menu will show.
  struct Config {
    Config();
    Config(Config&&);
    ~Config();
    Config& operator=(Config&&);

    views::MenuAnchorPosition anchor_position;

    // A list of the currently available lacros profiles. When this has two or
    // more elements, the menu will show the profiles, as well as an entry for
    // bringing up the profile manager.
    std::vector<LacrosProfileSummary> profiles;

    // Identifies the currently selected lacros profile. Only used when lacros
    // profiles are shown.
    uint64_t current_lacros_profile_id = 0;

    // Invoked with the lacros profile id if the user picks a profile.
    base::RepeatingCallback<void(uint64_t)> set_lacros_profile_id;

    // If set, the option to save the selected desk as a template is shown.
    std::optional<std::u16string> save_template_target_name;
    base::RepeatingClosure save_template_callback;

    // If set, the option to save the selected desk for later is shown.
    std::optional<std::u16string> save_later_target_name;
    base::RepeatingClosure save_later_callback;

    // If the menu option to combine desks is to be shown, then this, as well as
    // the callback, need to be set.
    std::optional<std::u16string> combine_desks_target_name;
    base::RepeatingClosure combine_desks_callback;

    // If set, the option to close all windows on the desk is shown.
    std::optional<std::u16string> close_all_target_name;
    base::RepeatingClosure close_all_callback;

    // Optional, invoked when the menu is closed.
    base::RepeatingClosure on_context_menu_closed_callback;
  };

  // An enum with identifiers to link context menu items to their associated
  // functions.
  enum CommandId {
    // Saves target desk as a template that can be repeatedly opened.
    kSaveAsTemplate = 1,
    // Saves target desk to be restored later.
    kSaveForLater,
    // Closes target desk and moves its windows to another desk.
    kCombineDesks,
    // Saves target desk in DesksController and gives user option to undo the
    // desk before the desk is fully removed and its windows are closed.
    kCloseAll,
    // Shows the lacros profile manager. Only available when desk profiles is
    // enabled.
    kShowProfileManager,
    // Start of dynamic IDs used for lacros profiles.
    kDynamicProfileStart,
  };

  DeskActionContextMenu(Config config, DeskMiniView* mini_view);
  DeskActionContextMenu(const DeskActionContextMenu&) = delete;
  DeskActionContextMenu& operator=(const DeskActionContextMenu&) = delete;
  ~DeskActionContextMenu() override;

  views::MenuItemView* root_menu_item_view() { return root_menu_item_view_; }

  // Closes the context menu if one is running.
  void MaybeCloseMenu();

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  void MenuClosed(ui::SimpleMenuModel* menu) override;

 private:
  friend class DesksTestApi;

  // Invokes `config_.set_lacros_profile_id` if `command_id` refers to a lacros
  // profile.
  void MaybeSetLacrosProfileId(int command_id);

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  Config config_;

  // Cache the `DeskMiniView` this menu is associated with, so we can use it to
  // access the `OverviewGrid` later.
  raw_ptr<DeskMiniView> mini_view_;

  ui::SimpleMenuModel context_menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  // The root menu item view. Cached for tooltips and accessible names.
  raw_ptr<views::MenuItemView> root_menu_item_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_ACTION_CONTEXT_MENU_H_
