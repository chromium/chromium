// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_nudge_controller.h"
#include "base/memory/weak_ptr.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class ClipboardHistory;
class ClipboardHistoryMenuModelAdapter;
class ClipboardHistoryResourceManager;

// Shows a menu with the last few things saved in the clipboard when the
// keyboard shortcut is pressed.
class ASH_EXPORT ClipboardHistoryController {
 public:
  ClipboardHistoryController();
  ClipboardHistoryController(const ClipboardHistoryController&) = delete;
  ClipboardHistoryController& operator=(const ClipboardHistoryController&) =
      delete;
  ~ClipboardHistoryController();

  void Init();

  // Returns if the contextual menu is currently showing.
  bool IsMenuShowing() const;

  // Returns bounds for the contextual menu in screen coordinates.
  gfx::Rect GetMenuBoundsInScreenForTest() const;

  // Returns the history which tracks what is being copied to the clipboard.
  const ClipboardHistory* history() const { return clipboard_history_.get(); }

  // Do not use it. Will be removed soon.
  const std::vector<ClipboardHistoryItem>& clipboard_items() const {
    return clipboard_items_;
  }

  // Returns the resource manager which gets labels and images for items copied
  // to the clipboard.
  const ClipboardHistoryResourceManager* resource_manager() const {
    return resource_manager_.get();
  }

  ClipboardNudgeController* nudge_controller() const {
    return nudge_controller_.get();
  }

 private:
  class AcceleratorTarget;
  class MenuDelegate;

  bool CanShowMenu() const;
  void ShowMenu();
  void ExecuteSelectedMenuItem(int event_flags);
  void MenuOptionSelected(int index, int event_flags);

  gfx::Rect CalculateAnchorRect() const;

  // The menu being shown.
  std::unique_ptr<ClipboardHistoryMenuModelAdapter> context_menu_;
  // Used to keep track of what is being copied to the clipboard.
  std::unique_ptr<ClipboardHistory> clipboard_history_;
  // Manages resources for clipboard history.
  std::unique_ptr<ClipboardHistoryResourceManager> resource_manager_;
  // Detects the search+v key combo.
  std::unique_ptr<AcceleratorTarget> accelerator_target_;
  // Handles events on the contextual menu.
  std::unique_ptr<MenuDelegate> menu_delegate_;
  // The items we show in the contextual menu. Saved so we can paste them later.
  std::vector<ClipboardHistoryItem> clipboard_items_;
  // Controller that shows contextual nudges for multipaste.
  std::unique_ptr<ClipboardNudgeController> nudge_controller_;

  base::WeakPtrFactory<ClipboardHistoryController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_H_
