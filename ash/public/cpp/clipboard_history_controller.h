// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CLIPBOARD_HISTORY_CONTROLLER_H_
#define ASH_PUBLIC_CPP_CLIPBOARD_HISTORY_CONTROLLER_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/base/ui_base_types.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {
class ClipboardHistoryItem;
class ScopedClipboardHistoryPause;

// An interface implemented in Ash to enable the Chrome side to show the
// clipboard history menu.
class ASH_PUBLIC_EXPORT ClipboardHistoryController {
 public:
  using GetHistoryValuesCallback =
      base::OnceCallback<void(std::vector<ClipboardHistoryItem>)>;
  using OnMenuClosingCallback = base::OnceCallback<void(bool will_paste_item)>;

  class Observer : public base::CheckedObserver {
   public:
    // Called when the clipboard history menu is shown.
    virtual void OnClipboardHistoryMenuShown(
        crosapi::mojom::ClipboardHistoryControllerShowSource show_source) {}

    // Called when the user pastes from the clipboard history menu.
    virtual void OnClipboardHistoryPasted() {}

    // Called when:
    // 1. item(s) are added to, removed from, or updated in the clipboard
    // history; or
    // 2. clipboard history's availability (based on the session state and the
    // login status) changes.
    // NOTE: Observers will only be notified once about an atomic update
    // affecting multiple items, e.g., adding a new item and the oldest item
    // being removed as a result.
    virtual void OnClipboardHistoryItemsUpdated() {}
  };

  // Returns the singleton instance.
  static ClipboardHistoryController* Get();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns whether clipboard history is enabled and non-empty.
  virtual bool HasAvailableHistoryItems() const = 0;

  // Attempts to show the clipboard history menu triggered by `source_type` at
  // the position specified by `anchor_rect`. Returns whether the menu was
  // shown. `show_source` indicates how the user opened the menu. As long as the
  // menu is shown, `callback` runs just before the menu closes to indicate
  // whether a clipboard history paste is imminent.
  virtual bool ShowMenu(
      const gfx::Rect& anchor_rect,
      ui::MenuSourceType source_type,
      crosapi::mojom::ClipboardHistoryControllerShowSource show_source) = 0;
  virtual bool ShowMenu(
      const gfx::Rect& anchor_rect,
      ui::MenuSourceType source_type,
      crosapi::mojom::ClipboardHistoryControllerShowSource show_source,
      OnMenuClosingCallback callback) = 0;

  // Notify the clipboard history that a screenshot notification was created.
  virtual void OnScreenshotNotificationCreated() = 0;

  // Creates a ScopedClipboardHistoryPause, which pauses ClipboardHistory for
  // its lifetime.
  virtual std::unique_ptr<ScopedClipboardHistoryPause> CreateScopedPause() = 0;

  // Calls `callback` with the clipboard history list, which tracks what has
  // been copied to the clipboard. If clipboard history is disabled in the
  // current mode, `callback` will be called with an empty history list.
  virtual void GetHistoryValues(GetHistoryValuesCallback callback) const = 0;

  // Returns a list of ids for items in the clipboard history, if clipboard
  // history is enabled. Otherwise, returns an empty list.
  virtual std::vector<std::string> GetHistoryItemIds() const = 0;

  // Pastes the clipboard item specified by `item_id` from `paste_source`.
  virtual bool PasteClipboardItemById(
      const std::string& item_id,
      int event_flags,
      crosapi::mojom::ClipboardHistoryControllerShowSource paste_source) = 0;

  // Deletes the clipboard item specified by the item id.
  virtual bool DeleteClipboardItemById(const std::string& item_id) = 0;

 protected:
  ClipboardHistoryController();
  virtual ~ClipboardHistoryController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CLIPBOARD_HISTORY_CONTROLLER_H_
