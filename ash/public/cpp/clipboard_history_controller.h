// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CLIPBOARD_HISTORY_CONTROLLER_H_
#define ASH_PUBLIC_CPP_CLIPBOARD_HISTORY_CONTROLLER_H_

#include <memory>
#include <set>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/base/ui_base_types.h"

namespace base {
class Value;
class UnguessableToken;
}  // namespace base

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {
class ScopedClipboardHistoryPause;

// An interface implemented in Ash to enable the Chrome side to show the
// clipboard history menu.
class ASH_PUBLIC_EXPORT ClipboardHistoryController {
 public:
  using GetHistoryValuesCallback = base::OnceCallback<void(base::Value)>;

  class Observer : public base::CheckedObserver {
   public:
    // Called when the clipboard history menu is shown.
    virtual void OnClipboardHistoryMenuShown() {}
    // Called when the user pastes from the clipboard history menu.
    virtual void OnClipboardHistoryPasted() {}
    // Called when the clipboard history changes.
    virtual void OnClipboardHistoryItemListAddedOrRemoved() {}
    // Called when existing clipboard items in the history have changes.
    // virtual void OnClipboardHistoryItemsUpdated(
    virtual void OnClipboardHistoryItemsUpdated(
        const std::vector<base::UnguessableToken>& menu_item_ids) {}
  };

  // Returns the singleton instance.
  static ClipboardHistoryController* Get();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns whether the clipboard history menu is able to show.
  virtual bool CanShowMenu() const = 0;

  // Shows the clipboard history menu triggered by `source_type` at the
  // specified position.
  virtual void ShowMenu(
      const gfx::Rect& anchor_rect,
      ui::MenuSourceType source_type,
      crosapi::mojom::ClipboardHistoryControllerShowSource show_source) = 0;

  // Notify the clipboard history that a screenshot notification was created.
  virtual void OnScreenshotNotificationCreated() = 0;

  // Creates a ScopedClipboardHistoryPause, which pauses ClipboardHistory for
  // its lifetime.
  virtual std::unique_ptr<ScopedClipboardHistoryPause> CreateScopedPause() = 0;

  // Calls `callback` with the clipboard history list, which tracks what has
  // been copied to the clipboard. Only the items listed in |item_id_filter| are
  // returned. If |item_id_filter| is empty, then all items in the history are
  // returned. If clipboard history is disabled in the current mode, `callback`
  // will be called with an empty history list.
  // TODO(crbug.com/1309666): Remove const ref from |item_id_filter| param type.
  virtual void GetHistoryValues(const std::set<std::string>& item_id_filter,
                                GetHistoryValuesCallback callback) const = 0;

  // Returns a list of item ids for items contained in the clipboard history.
  virtual std::vector<std::string> GetHistoryItemIds() const = 0;

  // Pastes the clipboard item specified by the item id.
  virtual bool PasteClipboardItemById(const std::string& item_id) = 0;

  // Deletes the clipboard item specified by the item id.
  virtual bool DeleteClipboardItemById(const std::string& item_id) = 0;

 protected:
  ClipboardHistoryController();
  virtual ~ClipboardHistoryController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CLIPBOARD_HISTORY_CONTROLLER_H_
