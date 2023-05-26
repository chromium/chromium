// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_application_menu_model.h"

#include <algorithm>
#include <limits>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/favicon_size.h"

namespace ash {

ShelfApplicationMenuModel::ShelfApplicationMenuModel(
    const std::u16string& title,
    const Items& items,
    ShelfItemDelegate* delegate)
    : ui::SimpleMenuModel(this), delegate_(delegate) {
  AddTitle(title);
  for (const auto& item : items) {
    enabled_commands_.emplace(item.command_id);
    AddItemWithIcon(item.command_id, item.title,
                    ui::ImageModel::FromImageSkia(item.icon));
  }
  AddSeparator(ui::SPACING_SEPARATOR);
  DCHECK_EQ(GetItemCount(), items.size() + 2) << "Update metrics |- 2|";
}

ShelfApplicationMenuModel::~ShelfApplicationMenuModel() = default;

bool ShelfApplicationMenuModel::IsCommandIdEnabled(int command_id) const {
  // This enables items added in the constructor, but not the title.
  return enabled_commands_.contains(command_id);
}

void ShelfApplicationMenuModel::ExecuteCommand(int command_id,
                                               int event_flags) {
  DCHECK(IsCommandIdEnabled(command_id));
  // Have the delegate execute its own custom command id for the given item.
  if (delegate_) {
    // Record app launch when selecting window to open from disambiguation
    // menu.
    Shell::Get()->app_list_controller()->RecordShelfAppLaunched();

    // The display hosting the menu is irrelevant, windows activate in-place.
    delegate_->ExecuteCommand(false /*from_context_menu*/, command_id,
                              event_flags, display::kInvalidDisplayId);
  }
  // Subtract two to avoid counting the title and separator.
  RecordMenuItemSelectedMetrics(command_id,
                                std::max(GetItemCount(), size_t{2}) - 2);
}

void ShelfApplicationMenuModel::RecordMenuItemSelectedMetrics(
    int command_id,
    int num_menu_items_enabled) {
  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.Menu.SelectedMenuItemIndex", command_id);
  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.Menu.NumItemsEnabledUponSelection",
                           num_menu_items_enabled);
}

}  // namespace ash
