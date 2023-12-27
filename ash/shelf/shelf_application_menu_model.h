// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_APPLICATION_MENU_MODEL_H_
#define ASH_SHELF_SHELF_APPLICATION_MENU_MODEL_H_

#include <memory>
#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class ShelfItemDelegate;

// A menu model listing open applications associated with a shelf item. Layout:
// +---------------------------+
// |                           |
// |         Shelf Item Title  |
// |                           |
// |  [Icon] Menu Item Label   |
// |  [Icon] Menu Item Label   |
// |                           |
// +---------------------------+
class ASH_EXPORT ShelfApplicationMenuModel
    : public ui::SimpleMenuModel,
      public ui::SimpleMenuModel::Delegate {
 public:
  using Items = ShelfItemDelegate::AppMenuItems;

  // Makes a menu with a |title|, |items|, and a separator for |delegate|.
  // |delegate| may be null in unit tests that do not execute commands.
  ShelfApplicationMenuModel(const std::u16string& title,
                            const Items& items,
                            ShelfItemDelegate* delegate);

  ShelfApplicationMenuModel(const ShelfApplicationMenuModel&) = delete;
  ShelfApplicationMenuModel& operator=(const ShelfApplicationMenuModel&) =
      delete;

  ~ShelfApplicationMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  friend class ShelfApplicationMenuModelTestAPI;

  // Records UMA metrics when a menu item is selected.
  void RecordMenuItemSelectedMetrics(int command_id,
                                     int num_menu_items_enabled);

  // The shelf item delegate that created the menu and executes its commands.
  const raw_ptr<ShelfItemDelegate> delegate_;

  // A set containing the enabled command IDs.
  base::flat_set<int> enabled_commands_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_APPLICATION_MENU_MODEL_H_
