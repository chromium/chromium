// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONTEXT_MENU_MODEL_H_
#define ASH_SHELF_SHELF_CONTEXT_MENU_MODEL_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

class ShelfItemDelegate;

// A context menu shown for shelf items, the shelf itself, or the desktop area.
// In tablet mode the "desktop area" includes the app list.
class ASH_EXPORT ShelfContextMenuModel : public ui::SimpleMenuModel,
                                         public ui::SimpleMenuModel::Delegate {
 public:
  // The command ids for Ash's shelf and wallpaper context menu items.
  // These are used in histograms, do not remove/renumber entries. Only add at
  // the end just before MENU_LOCAL_END. If you're adding to this enum with the
  // intention that it will be logged, add checks to ensure stability of the
  // enum and update the ChromeOSUICommands enum listing in
  // tools/metrics/histograms/enums.xml.
  enum CommandId {
    MENU_ASH_START = 500,  // Offset to avoid conflicts with other menus.
    MENU_AUTO_HIDE = MENU_ASH_START,
    MENU_ALIGNMENT_MENU = 501,
    MENU_ALIGNMENT_LEFT = 502,
    MENU_ALIGNMENT_RIGHT = 503,
    MENU_ALIGNMENT_BOTTOM = 504,
    // Note: Value 505 was for MENU_CHANGE_WALLPAPER. Do not reuse.
    MENU_PERSONALIZATION_HUB = 506,
    MENU_HIDE_CONTINUE_SECTION = 507,
    MENU_SHOW_CONTINUE_SECTION = 508,
    MENU_HIDE_DESK_NAME = 509,
    MENU_SHOW_DESK_NAME = 510,
    MENU_ASH_END
  };

  ShelfContextMenuModel(ShelfItemDelegate* delegate,
                        int64_t display_id,
                        bool menu_in_shelf);

  ShelfContextMenuModel(const ShelfContextMenuModel&) = delete;
  ShelfContextMenuModel& operator=(const ShelfContextMenuModel&) = delete;

  ~ShelfContextMenuModel() override;

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Add shelf auto-hide, shelf alignment, and wallpaper context menu items.
  void AddShelfAndWallpaperItems();

  raw_ptr<ShelfItemDelegate> delegate_;
  const int64_t display_id_;
  const bool menu_in_shelf_;
  std::unique_ptr<ui::SimpleMenuModel> alignment_submenu_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONTEXT_MENU_MODEL_H_
