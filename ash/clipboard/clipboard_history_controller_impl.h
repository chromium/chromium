// Copyright 2020 The Chromium Authors
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
#include "ash/clipboard/clipboard_history_resource_manager.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/one_shot_event.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"

class PrefRegistrySimple;

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class ClipboardHistoryControllerDelegate;
class ClipboardHistoryItem;
class ClipboardHistoryMenuModelAdapter;
class ClipboardHistoryResourceManager;
class ClipboardHistoryUrlTitleFetcher;
class ClipboardImageModelFactory;
class ClipboardNudgeController;
class ScopedClipboardHistoryPause;
enum class LoginStatus;

// Shows a menu with the last few things saved in the clipboard when the
// keyboard shortcut is pressed.
class ASH_EXPORT ClipboardHistoryControllerImpl
    : public ClipboardHistoryController,
      public ClipboardHistory::Observer,
      public ClipboardHistoryResourceManager::Observer,
      public SessionObserver {
 public:
  // Source and plain vs. rich text info for each paste. These values are used
  // in the Ash.ClipboardHistory.PasteType histogram and therefore cannot be
  // reordered. New types may be appended to the end of this enumeration.
  enum class ClipboardHistoryPasteType {
    kPlainTextAccelerator = 0,      // Plain text paste triggered by accelerator
    kRichTextAccelerator = 1,       // Rich text paste triggered by accelerator
    kPlainTextKeystroke = 2,        // Plain text paste triggered by keystroke
    kRichTextKeystroke = 3,         // Rich text paste triggered by keystroke
    kPlainTextMouse = 4,            // Plain text paste triggered by mouse click
    kRichTextMouse = 5,             // Rich text paste triggered by mouse click
    kPlainTextTouch = 6,            // Plain text paste triggered by gesture tap
    kRichTextTouch = 7,             // Rich text paste triggered by gesture tap
    kPlainTextVirtualKeyboard = 8,  // Plain text paste triggered by VK request
    kRichTextVirtualKeyboard = 9,   // Rich text paste triggered by VK request
    kPlainTextCtrlV = 10,           // Plain text paste triggered by Ctrl+V
    kRichTextCtrlV = 11,            // Rich text paste triggered by Ctrl+V
    kMaxValue = 11
  };

  explicit ClipboardHistoryControllerImpl(
      std::unique_ptr<ClipboardHistoryControllerDelegate> delegate);
  ClipboardHistoryControllerImpl(const ClipboardHistoryControllerImpl&) =
      delete;
  ClipboardHistoryControllerImpl& operator=(
      const ClipboardHistoryControllerImpl&) = delete;
  ~ClipboardHistoryControllerImpl() override;

  // Registers clipboard history profile prefs with the specified `registry`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Clean up the child widgets prior to destruction.
  void Shutdown();

  // Returns if the contextual menu is currently showing.
  bool IsMenuShowing() const;

  // Shows or hides the clipboard history menu through the keyboard accelerator.
  // If the menu was already shown, pastes the selected menu item before hiding.
  // If the menu was not already shown and `is_plain_text_paste` is true the
  // menu will not be shown. The common case for `is_plain_text_paste` is to
  // allow pasting plain text when menu is already open, otherwise do not allow
  // the plain text shortcut to open the menu.
  void ToggleMenuShownByAccelerator(bool is_plain_text_paste);

  // ClipboardHistoryController:
  void AddObserver(ClipboardHistoryController::Observer* observer) override;
  void RemoveObserver(ClipboardHistoryController::Observer* observer) override;
  bool ShowMenu(const gfx::Rect& anchor_rect,
                ui::MenuSourceType source_type,
                crosapi::mojom::ClipboardHistoryControllerShowSource
                    show_source) override;
  bool ShowMenu(
      const gfx::Rect& anchor_rect,
      ui::MenuSourceType source_type,
      crosapi::mojom::ClipboardHistoryControllerShowSource show_source,
      OnMenuClosingCallback callback) override;
  void GetHistoryValues(GetHistoryValuesCallback callback) const override;

  // Whether the clipboard history has items.
  bool IsEmpty() const;

  // Fires the timer to notify observers of item updates immediately.
  void FireItemUpdateNotificationTimerForTest();

  // Returns bounds for the contextual menu in screen coordinates.
  gfx::Rect GetMenuBoundsInScreenForTest() const;

  // Used to delay the post-encoding step of `GetHistoryValues()` until the
  // completion of some work that needs to happen after history values have been
  // requested and before the values are returned.
  void BlockGetHistoryValuesForTest();
  void ResumeGetHistoryValuesForTest();

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
      std::optional<base::TimeDelta> delay) {
    buffer_restoration_delay_for_test_ = delay;
  }

  void set_initial_item_selected_callback_for_test(
      base::RepeatingClosure new_callback) {
    initial_item_selected_callback_for_test_ = new_callback;
  }

  void set_confirmed_operation_callback_for_test(
      base::RepeatingCallback<void(bool)> new_callback) {
    confirmed_operation_callback_for_test_ = new_callback;
  }

  void set_new_bitmap_to_write_while_encoding_for_test(const SkBitmap& bitmap) {
    new_bitmap_to_write_while_encoding_for_test_ = bitmap;
  }

 private:
  class AcceleratorTarget;
  class MenuDelegate;

  // ClipboardHistoryController:
  bool HasAvailableHistoryItems() const override;
  void OnScreenshotNotificationCreated() override;
  std::unique_ptr<ScopedClipboardHistoryPause> CreateScopedPause() override;
  std::vector<std::string> GetHistoryItemIds() const override;
  bool PasteClipboardItemById(
      const std::string& item_id,
      int event_flags,
      crosapi::mojom::ClipboardHistoryControllerShowSource paste_source)
      override;
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

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnLoginStatusChanged(LoginStatus login_status) override;

  // Posts a task to notify `observers_` of updates to clipboard history items.
  void PostItemUpdateNotificationTask();

  // Notifies `observers_` of updates to clipboard history items. No-op if
  // there are no available clipboard history items and there were no available
  // history items in the last notification.
  void MaybeNotifyObserversOfItemUpdate();

  // Invoked by `GetHistoryValues()` once all clipboard instances with images
  // have been encoded into PNGs. Calls `callback` with the clipboard history
  // list, which tracks what has been copied to the clipboard. If clipboard
  // history is disabled in the current mode, `callback` will be called with an
  // empty history list.
  void GetHistoryValuesWithEncodedPNGs(
      GetHistoryValuesCallback callback,
      std::unique_ptr<std::map<base::UnguessableToken, std::vector<uint8_t>>>
          encoded_pngs);

  // Executes the command specified by `command_id` with the given
  // `event_flags`.
  void ExecuteCommand(int command_id, int event_flags);

  // Pastes the clipboard data of the clipboard history context menu item
  // specified by `command_id`. NOTE: This function assumes that the clipboard
  // history context menu has been open. It is different from
  // `PasteClipboardItemById()` that can be called without showing the clipboard
  // history context menu.
  void PasteClipboardItemByCommandId(int command_id,
                                     ClipboardHistoryPasteType paste_type);

  // Posts a task to paste `item` with `paste_type` to the active window, if
  // any.
  void MaybePostPasteTask(
      const ClipboardHistoryItem& item,
      ClipboardHistoryPasteType paste_type,
      crosapi::mojom::ClipboardHistoryControllerShowSource paste_source);

  // Pastes the specified clipboard history item, if `intended_window` matches
  // the active window. `paste_type` indicates the mode of paste execution for
  // metrics tracking as well as whether plain text should be pasted instead of
  // the full, rich-text clipboard data. `paste_source` indicates how the user
  // triggered the menu from which `item` was selected.
  void PasteClipboardHistoryItem(
      aura::Window* intended_window,
      ClipboardHistoryItem item,
      ClipboardHistoryPasteType paste_type,
      crosapi::mojom::ClipboardHistoryControllerShowSource paste_source);

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

  // Either the browser-implemented or test-implemented delegate depending on
  // whether we are running in an Ash-only test context.
  const std::unique_ptr<ClipboardHistoryControllerDelegate> delegate_;

  // The browser-implemented image model factory that renders html. This will be
  // `nullptr` if and only if we are running in an Ash-only test context.
  const std::unique_ptr<ClipboardImageModelFactory> image_model_factory_;

  // The browser-implemented URL title fetcher. This will be `nullptr` if and
  // only if we are running in an Ash-only test context.
  const std::unique_ptr<ClipboardHistoryUrlTitleFetcher> url_title_fetcher_;

  // Observers notified when clipboard history is shown, used, or updated.
  base::ObserverList<ClipboardHistoryController::Observer> observers_;

  // Used to keep track of what is being copied to the clipboard.
  std::unique_ptr<ClipboardHistory> clipboard_history_;
  // Manages resources for clipboard history.
  std::unique_ptr<ClipboardHistoryResourceManager> resource_manager_;
  // Detects the search+v key combo.
  std::unique_ptr<AcceleratorTarget> accelerator_target_;
  // Controller that shows contextual nudges for multipaste.
  std::unique_ptr<ClipboardNudgeController> nudge_controller_;
  // Context menu displayed by `ShowMenu()`. Null when `MenuIsShowing()` is
  // false.
  std::unique_ptr<ClipboardHistoryMenuModelAdapter> context_menu_;
  // Handles events on the `context_menu_`.
  std::unique_ptr<MenuDelegate> menu_delegate_;

  // How the user last caused the `context_menu_` to show.
  crosapi::mojom::ClipboardHistoryControllerShowSource last_menu_source_;

  // Indicates whether the clipboard data has been replaced due to an
  // in-progress clipboard history paste.
  bool clipboard_data_replaced_ = false;

  // Used to post asynchronous tasks when opening or closing the clipboard
  // history menu. Note that those tasks have data races between each other.
  // The timer can guarantee that at most one task is alive.
  base::OneShotTimer menu_task_timer_;

  // Indicates the count of pastes which are triggered through the clipboard
  // history menu and are waiting for the confirmations from `ClipboardHistory`.
  int pastes_to_be_confirmed_ = 0;

  // Used to post a task to notify `observers_` of updates to clipboard history
  // items.
  base::OneShotTimer item_update_notification_timer_;

  // True if there were available items in the last clipboard history update
  // notification.
  bool has_available_items_in_last_update_ = false;

  // Created when a test requests that `GetHistoryValues()` wait for some work
  // to be done before encoding finishes. Reset and recreated if the same test
  // makes the request to pause `GetHistoryValues()` again.
  std::unique_ptr<base::OneShotEvent> get_history_values_blocker_for_test_;

  // The delay interval for restoring the clipboard buffer to its original
  // state following a paste event.
  std::optional<base::TimeDelta> buffer_restoration_delay_for_test_;

  // Called when the first item view is selected after the clipboard history
  // menu opens.
  base::RepeatingClosure initial_item_selected_callback_for_test_;

  // Called when a copy or paste finishes. Accepts the operation's success as an
  // argument.
  base::RepeatingCallback<void(bool)> confirmed_operation_callback_for_test_;

  // A new bitmap to be written to the clipboard while existing images are being
  // encoded during `GetHistoryValues()`, which will force `GetHistoryValues()`
  // to re-run in order to encode this new bitmap. This member is marked mutable
  // so it can be cleared once it has been written to the clipboard.
  mutable SkBitmap new_bitmap_to_write_while_encoding_for_test_;

  base::WeakPtrFactory<ClipboardHistoryControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_IMPL_H_
