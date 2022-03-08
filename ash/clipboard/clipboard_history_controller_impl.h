// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_IMPL_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_resource_manager.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"

namespace aura {
class Window;
}  // namespace aura

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
      public ClipboardHistory::Observer,
      public ClipboardHistoryResourceManager::Observer {
 public:
  ClipboardHistoryControllerImpl();
  ClipboardHistoryControllerImpl(const ClipboardHistoryControllerImpl&) =
      delete;
  ClipboardHistoryControllerImpl& operator=(
      const ClipboardHistoryControllerImpl&) = delete;
  ~ClipboardHistoryControllerImpl() override;

  // Clean up th child widgets prior to destruction.
  void Shutdown();

  void AddObserver(
      ClipboardHistoryController::Observer* observer) const override;
  void RemoveObserver(
      ClipboardHistoryController::Observer* observer) const override;

  // Returns if the contextual menu is currently showing.
  bool IsMenuShowing() const;

  // Shows the clipboard history menu through the keyboard accelerator.
  void ShowMenuByAccelerator();

  // ClipboardHistoryController:
  void ShowMenu(const gfx::Rect& anchor_rect,
                ui::MenuSourceType source_type,
                crosapi::mojom::ClipboardHistoryControllerShowSource
                    show_source) override;

  // Returns bounds for the contextual menu in screen coordinates.
  gfx::Rect GetMenuBoundsInScreenForTest() const;

  void GetHistoryValuesForTest(GetHistoryValuesCallback callback) const;

  // Whether the ClipboardHistory has items.
  bool IsEmpty() const;

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

  void set_buffer_restoration_delay_for_test(
      absl::optional<base::TimeDelta> delay) {
    buffer_restoration_delay_for_test_ = delay;
  }

  void set_initial_item_selected_callback_for_test(
      base::RepeatingClosure new_callback) {
    initial_item_selected_callback_for_test_ = new_callback;
  }

  void set_confirmed_operation_callback_for_test(
      base::RepeatingClosure new_callback) {
    confirmed_operation_callback_for_test_ = new_callback;
  }

  void set_new_bitmap_to_write_while_encoding_for_test(const SkBitmap& bitmap) {
    new_bitmap_to_write_while_encoding_for_test_ = bitmap;
  }

 private:
  class AcceleratorTarget;
  class MenuDelegate;

  // ClipboardHistoryController:
  bool CanShowMenu() const override;
  bool ShouldShowNewFeatureBadge() const override;
  void MarkNewFeatureBadgeShown() override;
  void OnScreenshotNotificationCreated() override;
  std::unique_ptr<ScopedClipboardHistoryPause> CreateScopedPause() override;
  void GetHistoryValues(const std::set<std::string>& item_id_filter,
                        GetHistoryValuesCallback callback) const override;
  std::vector<std::string> GetHistoryItemIds() const override;
  bool PasteClipboardItemById(const std::string& item_id) override;
  bool DeleteClipboardItemById(const std::string& item_id) override;

  // ClipboardHistory::Observer:
  void OnClipboardHistoryItemAdded(const ClipboardHistoryItem& item,
                                   bool is_duplicate) override;
  void OnClipboardHistoryItemRemoved(const ClipboardHistoryItem& item) override;
  void OnClipboardHistoryCleared() override;
  void OnOperationConfirmed(bool copy) override;

  // ClipboardHistoryResourceManager:
  void OnCachedImageModelUpdated(
      const std::vector<base::UnguessableToken>& menu_item_ids) override;

  // Invoked by GetHistoryValues once all clipboard instances with images have
  // been encoded into PNGs. Returns the history which tracks what is being
  // copied to the clipboard. Only the items listed in `item_id_filter` are
  // returned. If `item_id_filter` is empty, then all items in the history are
  // returned.
  void GetHistoryValuesWithEncodedPNGs(
      const std::set<std::string>& item_id_filter,
      GetHistoryValuesCallback callback,
      std::unique_ptr<std::map<base::UnguessableToken, std::vector<uint8_t>>>
          encoded_pngs);

  void ExecuteSelectedMenuItem(int event_flags);

  // Executes the command specified by `command_id` with the given event flags.
  void ExecuteCommand(int command_id, int event_flags);

  // Paste the clipboard data of the menu item specified by `command_id`.
  // `paste_plain_text` indicates whether the plain text instead of the
  // clipboard data should be pasted.
  void PasteMenuItemData(int command_id, bool paste_plain_text);

  // Pastes the specified clipboard history item, if |intended_window| matches
  // the active window.
  void PasteClipboardHistoryItem(aura::Window* intended_window,
                                 ClipboardHistoryItem item,
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

  // Used to post asynchronous tasks when opening or closing the clipboard
  // history menu. Note that those tasks have data races between each other.
  // The timer can guarantee that at most one task is alive.
  base::OneShotTimer menu_task_timer_;

  // Indicates the count of pastes which are triggered through the clipboard
  // history menu and are waiting for the confirmations from `ClipboardHistory`.
  int pastes_to_be_confirmed_ = 0;

  // The delay interval for restoring the clipboard buffer to its original
  // state following a paste event.
  absl::optional<base::TimeDelta> buffer_restoration_delay_for_test_;

  // Called when the first item view is selected after the clipboard history
  // menu opens.
  base::RepeatingClosure initial_item_selected_callback_for_test_;

  // Called when the controller is notified of the confirmed clipboard data
  // operation.
  base::RepeatingClosure confirmed_operation_callback_for_test_;

  // A new bitmap to be written to the clipboard while existing images are being
  // encoded during `GetHistoryValues()`, which will force `GetHistoryValues()`
  // to re-run in order to encode this new bitmap. This member is marked mutable
  // so it can be cleared once it has been written to the clipboard.
  mutable SkBitmap new_bitmap_to_write_while_encoding_for_test_;

  base::WeakPtrFactory<ClipboardHistoryControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_IMPL_H_
