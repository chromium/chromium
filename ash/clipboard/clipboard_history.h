// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_

#include <deque>
#include <list>

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/token.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_observer.h"

namespace ash {
class ScopedClipboardHistoryPauseImpl;

namespace clipboard_history_util {
enum class PauseBehavior;
}  // namespace clipboard_history_util

// Keeps track of the last few things saved in the clipboard.
class ASH_EXPORT ClipboardHistory : public ui::ClipboardObserver {
 public:
  class ASH_EXPORT Observer : public base::CheckedObserver {
   public:
    // Called when a `ClipboardHistoryItem` has been added. `is_duplicate` is
    // true if `item` is already in clipboard history when adding.
    virtual void OnClipboardHistoryItemAdded(const ClipboardHistoryItem& item,
                                             bool is_duplicate) {}

    // Called when a ClipboardHistoryItem has been removed.
    virtual void OnClipboardHistoryItemRemoved(
        const ClipboardHistoryItem& item) {}

    // Called when ClipboardHistory is Clear()-ed.
    virtual void OnClipboardHistoryCleared() {}

    // Called when the operation on clipboard data is confirmed.
    virtual void OnOperationConfirmed(bool copy) {}
  };

  ClipboardHistory();
  ClipboardHistory(const ClipboardHistory&) = delete;
  ClipboardHistory& operator=(const ClipboardHistory&) = delete;
  ~ClipboardHistory() override;

  void AddObserver(Observer* observer) const;
  void RemoveObserver(Observer* observer) const;

  // Returns the list of most recent items. The returned list is sorted by
  // recency.
  const std::list<ClipboardHistoryItem>& GetItems() const;
  std::list<ClipboardHistoryItem>& GetItems();

  // Deletes every item in the clipboard history. The clipboard is cleared as
  // well to ensure that its contents stay in sync with the first item in the
  // clipboard history.
  void Clear();

  // Returns whether the clipboard history of the active account is empty.
  bool IsEmpty() const;

  // Remove the item specified by `id`. If the target item does not exist,
  // do nothing.
  void RemoveItemForId(const base::UnguessableToken& id);

  // ui::ClipboardObserver:
  void OnClipboardDataChanged() override;
  void OnClipboardDataRead() override;

  base::WeakPtr<ClipboardHistory> GetWeakPtr();

 private:
  // Friended to allow `ScopedClipboardHistoryPauseImpl` to `Pause()` and
  // `Resume()`.
  // TODO(b/269470292): Use a `PassKey` for this.
  friend class ScopedClipboardHistoryPauseImpl;

  // Ensures that the clipboard buffer contains the same data as the item at the
  // top of clipboard history. If clipboard history is empty, then the clipboard
  // is cleared.
  void SyncClipboardToClipboardHistory();

  // Adds `data` to the top of the history list if `data` is supported by
  // clipboard history. If `data` is not supported, this method no-ops. If
  // `data` is already in the history list, `data` will be moved to the top of
  // the list.
  void MaybeCommitData(ui::ClipboardData data, bool is_reorder_on_paste);

  // When `Pause()` is called, clipboard accesses will modify clipboard history
  // according to `pause_behavior` until `Resume()` is called with that pause's
  // `pause_id`. If `Pause()` is called while another pause is active, the
  // newest pause's behavior will be respected. When the newest pause ends, the
  // next newest pause's behavior will be restored.
  const base::Token& Pause(
      clipboard_history_util::PauseBehavior pause_behavior);
  void Resume(const base::Token& pause_id);
  struct PauseInfo {
    base::Token pause_id;
    clipboard_history_util::PauseBehavior pause_behavior;
  };

  // Keeps track of consecutive clipboard operations and records metrics.
  void OnClipboardOperation(bool copy);

  // Active clipboard history pauses, stored in LIFO order so that the newest
  // pause dictates behavior. Rather than a stack, we use a deque where the
  // newest pause is added to and removed from the front. Not using a stack
  // allows us to find and remove the correct pause in cases where pauses are
  // not destroyed in LIFO order, and adding to the front of the deque rather
  // than the back allows us to iterate forward when searching for the correct
  // pause, simplifying removal logic.
  std::deque<PauseInfo> pauses_;

  // The number of consecutive copies, reset after a paste.
  int consecutive_copies_ = 0;

  // The number of consecutive pastes, reset after a copy.
  int consecutive_pastes_ = 0;

  // The history of data copied to the Clipboard. Items of the list are sorted
  // by recency.
  std::list<ClipboardHistoryItem> history_list_;

  // Mutable to allow adding/removing from |observers_| through a const
  // ClipboardHistory.
  mutable base::ObserverList<Observer> observers_;

  // Factory to create WeakPtrs used to debounce calls to `CommitData()`.
  base::WeakPtrFactory<ClipboardHistory> commit_data_weak_factory_{this};

  // Factory to create WeakPtrs used to debounce calls to
  // `OnClipboardOperation()`.
  base::WeakPtrFactory<ClipboardHistory> clipboard_histogram_weak_factory_{
      this};

  // Factory to create WeakPtrs for ClipboardHistory.
  base::WeakPtrFactory<ClipboardHistory> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_
