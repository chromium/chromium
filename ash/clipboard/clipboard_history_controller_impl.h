// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_IMPL_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_IMPL_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_nudge_controller.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/memory/weak_ptr.h"

namespace views {
enum class MenuAnchorPosition;
}  // namespace views

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class ClipboardHistory;
class ClipboardHistoryMenuModelAdapter;
class ClipboardHistoryResourceManager;

// Shows a menu with the last few things saved in the clipboard when the
// keyboard shortcut is pressed.
class ASH_EXPORT ClipboardHistoryControllerImpl
    : public ClipboardHistoryController {
 public:
  ClipboardHistoryControllerImpl();
  ClipboardHistoryControllerImpl(const ClipboardHistoryControllerImpl&) =
      delete;
  ClipboardHistoryControllerImpl& operator=(
      const ClipboardHistoryControllerImpl&) = delete;
  ~ClipboardHistoryControllerImpl() override;

  void Init();

  // Returns if the contextual menu is currently showing.
  bool IsMenuShowing() const;

  // Returns bounds for the contextual menu in screen coordinates.
  gfx::Rect GetMenuBoundsInScreenForTest() const;

  // Returns the history which tracks what is being copied to the clipboard.
  const ClipboardHistory* history() const { return clipboard_history_.get(); }

  // Returns the resource manager which gets labels and images for items copied
  // to the clipboard.
  const ClipboardHistoryResourceManager* resource_manager() const {
    return resource_manager_.get();
  }

  ClipboardNudgeController* nudge_controller() const {
    return nudge_controller_.get();
  }

  const ClipboardHistoryMenuModelAdapter* context_menu_for_test() const {
    return context_menu_.get();
  }

 private:
  class AcceleratorTarget;
  class MenuDelegate;

  // ClipboardHistoryController:
  void ShowMenu(const gfx::Rect& anchor_rect,
                views::MenuAnchorPosition menu_anchor_position,
                ui::MenuSourceType source_type) override;
  bool CanShowMenu() const override;

  // Shows the clipboard history menu through the keyboard accelerator.
  void ShowMenuByAccelerator();

  void ExecuteSelectedMenuItem(int event_flags);
  void MenuOptionSelected(int command_id, int event_flags);

  // Delete the menu item being selected and its corresponding data. If no item
  // is selected, do nothing.
  void DeleteSelectedMenuItemIfAny();

  gfx::Rect CalculateAnchorRect() const;

  // Called when the contextual menu is closed.
  void OnMenuClosed();

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
  // Controller that shows contextual nudges for multipaste.
  std::unique_ptr<ClipboardNudgeController> nudge_controller_;

  base::WeakPtrFactory<ClipboardHistoryControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_IMPL_H_
