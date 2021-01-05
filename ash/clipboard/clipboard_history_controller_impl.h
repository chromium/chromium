// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_IMPL_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_IMPL_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class ClipboardHistoryMenuModelAdapter;
class ClipboardHistoryResourceManager;
class ClipboardNudgeController;
class ScopedClipboardHistoryPause;

// Shows a menu with the last few things saved in the clipboard when the
// keyboard shortcut is pressed.
class ASH_EXPORT ClipboardHistoryControllerImpl
    : public ClipboardHistoryController,
      public ClipboardHistory::Observer {
 public:
  ClipboardHistoryControllerImpl();
  ClipboardHistoryControllerImpl(const ClipboardHistoryControllerImpl&) =
      delete;
  ClipboardHistoryControllerImpl& operator=(
      const ClipboardHistoryControllerImpl&) = delete;
  ~ClipboardHistoryControllerImpl() override;

  void AddObserver(
      ClipboardHistoryController::Observer* observer) const override;
  void RemoveObserver(
      ClipboardHistoryController::Observer* observer) const override;

  // Returns if the contextual menu is currently showing.
  bool IsMenuShowing() const;

  // Shows the clipboard history menu through the keyboard accelerator.
  void ShowMenuByAccelerator();

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

  ClipboardHistoryMenuModelAdapter* context_menu_for_test() {
    return context_menu_.get();
  }

 private:
  class AcceleratorTarget;
  class MenuDelegate;

  // ClipboardHistoryController:
  bool CanShowMenu() const override;
  void ShowMenu(const gfx::Rect& anchor_rect,
                ui::MenuSourceType source_type) override;
  std::unique_ptr<ScopedClipboardHistoryPause> CreateScopedPause() override;
  base::Value GetHistoryValues(
      const std::set<std::string>& item_id_filter) const override;
  bool PasteClipboardItemById(const std::string& item_id) override;
  bool DeleteClipboardItemById(const std::string& item_id) override;

  // ClipboardHistory::Observer:
  void OnClipboardHistoryCleared() override;

  void ExecuteSelectedMenuItem(int event_flags);

  // Executes the command specified by `command_id` with the given event flags.
  void ExecuteCommand(int command_id, int event_flags);

  // Paste the clipboard data of the menu item specified by `command_id`.
  // `paste_plain_text` indicates whether the plain text instead of the
  // clipboard data should be pasted.
  void PasteMenuItemData(int command_id, bool paste_plain_text);

  // Pastes the specified clipboard history item.
  void PasteClipboardHistoryItem(const ClipboardHistoryItem& item,
                                 bool paste_plain_text);

  // Delete the menu item being selected and its corresponding data. If no item
  // is selected, do nothing.
  void DeleteSelectedMenuItemIfAny();

  // Delete the menu item specified by `command_id` and its corresponding data.
  void DeleteItemWithCommandId(int command_id);

  // Deletes the specified clipboard history item.
  void DeleteClipboardHistoryItem(const ClipboardHistoryItem& item);

  // Advances the pseudo focus (backward if `reverse` is true).
  void AdvancePseudoFocus(bool reverse);

  // Calculates the anchor rect for the ClipboardHistory menu.
  gfx::Rect CalculateAnchorRect() const;

  // Called when the contextual menu is closed.
  void OnMenuClosed();

  // Mutable to allow adding/removing from |observers_| through a const
  // ClipboardHistoryControllerImpl.
  mutable base::ObserverList<ClipboardHistoryController::Observer> observers_;

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

  // Whether a paste is currently being performed.
  bool currently_pasting_ = false;

  base::WeakPtrFactory<ClipboardHistoryControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_IMPL_H_
