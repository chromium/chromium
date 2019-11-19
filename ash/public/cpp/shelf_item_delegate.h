// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_ITEM_DELEGATE_H_
#define ASH_PUBLIC_CPP_SHELF_ITEM_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/event.h"

class AppWindowLauncherItemController;

namespace gfx {
class ImageSkia;
}

namespace ui {
class SimpleMenuModel;
}

namespace ash {

// ShelfItemDelegate tracks some item state, handles shelf item selection, menu
// command execution, etc.
class ASH_PUBLIC_EXPORT ShelfItemDelegate {
 public:
  explicit ShelfItemDelegate(const ShelfID& shelf_id);
  virtual ~ShelfItemDelegate();

  const ShelfID& shelf_id() const { return shelf_id_; }
  void set_shelf_id(const ShelfID& shelf_id) { shelf_id_ = shelf_id; }
  const std::string& app_id() const { return shelf_id_.app_id; }
  const std::string& launch_id() const { return shelf_id_.launch_id; }

  bool image_set_by_controller() const { return image_set_by_controller_; }
  void set_image_set_by_controller(bool image_set_by_controller) {
    image_set_by_controller_ = image_set_by_controller;
  }

  // Called when the user selects a shelf item. The event, display, and source
  // info should be provided if known; some implementations use these arguments.
  // Defaults: (nullptr, kInvalidDisplayId, LAUNCH_FROM_UNKNOWN)
  // The callback reports the action taken and any application menu to show.
  // NOTE: This codepath is not currently used for context menu triggering.
  using AppMenuItem = std::pair<base::string16, gfx::ImageSkia>;
  using AppMenuItems = std::vector<AppMenuItem>;
  using ItemSelectedCallback =
      base::OnceCallback<void(ShelfAction, AppMenuItems)>;
  virtual void ItemSelected(std::unique_ptr<ui::Event> event,
                            int64_t display_id,
                            ShelfLaunchSource source,
                            ItemSelectedCallback callback);

  // Returns items for the application menu; used for convenience and testing.
  virtual AppMenuItems GetAppMenuItems(int event_flags);

  // Returns the context menu model; used to show ShelfItem context menus.
  using GetContextMenuCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  virtual void GetContextMenu(int64_t display_id,
                              GetContextMenuCallback callback);

  // Returns nullptr if class is not AppWindowLauncherItemController.
  virtual AppWindowLauncherItemController* AsAppWindowLauncherItemController();

  // Attempts to execute a context menu command; returns true if it was run.
  bool ExecuteContextMenuCommand(int64_t command_id, int32_t event_flags);

  // Called on invocation of a shelf item's context or application menu command.
  // |from_context_menu| is true if the command came from a context menu, or
  // false if the command came from an application menu. If the |display_id| is
  // unknown or irrelevant, callers may pass |display::kInvalidDisplayId|.
  virtual void ExecuteCommand(bool from_context_menu,
                              int64_t command_id,
                              int32_t event_flags,
                              int64_t display_id) = 0;

  // Closes all windows associated with this shelf item.
  virtual void Close() = 0;

 private:
  // The shelf id; empty if there is no app associated with the item.
  // Besides the application id, ShelfID also contains a launch id, which is an
  // id that can be passed to an app when launched in order to support multiple
  // shelf items per app. This id is used together with the app_id to uniquely
  // identify each shelf item that has the same app_id.
  ShelfID shelf_id_;

  // Set to true if the launcher item image has been set by the controller.
  bool image_set_by_controller_ = false;

  // The context menu model that was last shown for the associated shelf item.
  std::unique_ptr<ui::SimpleMenuModel> context_menu_;

  base::WeakPtrFactory<ShelfItemDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ShelfItemDelegate);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_ITEM_DELEGATE_H_
