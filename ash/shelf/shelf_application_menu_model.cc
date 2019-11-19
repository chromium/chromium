// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_application_menu_model.h"

#include <algorithm>
#include <limits>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "ui/display/types/display_constants.h"

namespace ash {

ShelfApplicationMenuModel::ShelfApplicationMenuModel(
    const base::string16& title,
    Items items,
    ShelfItemDelegate* delegate)
    : ui::SimpleMenuModel(this), delegate_(delegate) {
  AddTitle(title);
  for (size_t i = 0; i < items.size(); i++)
    AddItemWithIcon(i, items[i].first, items[i].second);
  AddSeparator(ui::SPACING_SEPARATOR);
  DCHECK_EQ(GetItemCount(), int{items.size() + 2}) << "Update metrics |- 2|";
}

ShelfApplicationMenuModel::~ShelfApplicationMenuModel() = default;

bool ShelfApplicationMenuModel::IsCommandIdEnabled(int command_id) const {
  // This enables items added in the constructor, but not the title.
  return command_id >= 0 && command_id < GetItemCount();
}

void ShelfApplicationMenuModel::ExecuteCommand(int command_id,
                                               int event_flags) {
  DCHECK(IsCommandIdEnabled(command_id));
  // Have the delegate execute its own custom command id for the given item.
  if (delegate_) {
    // Record app launch when selecting window to open from disambiguation
    // menu.
    Shell::Get()->app_list_controller()->RecordShelfAppLaunched(
        base::nullopt /* recorded_app_list_view_state */,
        base::nullopt /* recorded_home_launcher_shown */);

    // The display hosting the menu is irrelevant, windows activate in-place.
    delegate_->ExecuteCommand(false /*from_context_menu*/, command_id,
                              event_flags, display::kInvalidDisplayId);
  }
  // Subtract two to avoid counting the title and separator.
  RecordMenuItemSelectedMetrics(command_id, std::max(GetItemCount() - 2, 0));
}

void ShelfApplicationMenuModel::RecordMenuItemSelectedMetrics(
    int command_id,
    int num_menu_items_enabled) {
  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.Menu.SelectedMenuItemIndex", command_id);
  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.Menu.NumItemsEnabledUponSelection",
                           num_menu_items_enabled);
}

}  // namespace ash
