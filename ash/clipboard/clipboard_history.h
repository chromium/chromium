// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_

#include <list>

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_observer.h"

namespace ash {

class ScopedClipboardHistoryPauseImpl;

// Keeps track of the last few things saved in the clipboard.
class ASH_EXPORT ClipboardHistory : public ui::ClipboardObserver {
 public:
  class ASH_EXPORT Observer : public base::CheckedObserver {
   public:
    // Called when a ClipboardHistoryItem has been added.
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

  // Deletes clipboard history. Does not modify content stored in the clipboard.
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
  // Friended to allow ScopedClipboardHistoryPauseImpl to `Pause()` and
  // `Resume()`.
  friend class ScopedClipboardHistoryPauseImpl;

  // Adds `data` to the top of the history list if `data` is supported by
  // clipboard history. If `data` is not supported, this method no-ops. If
  // `data` is already in the history list, `data` will be moved to the top of
  // the list.
  void MaybeCommitData(ui::ClipboardData data);

  // If `metrics_only` is true, pausing will not prevent modifications to
  // clipboard history, but it will prevent updates to the metrics tracked on
  // clipboard operations.
  void Pause(bool metrics_only);
  void Resume(bool metrics_only);

  // Keeps track of consecutive clipboard operations and records metrics.
  void OnClipboardOperation(bool copy);

  // The count of pauses.
  size_t num_pause_ = 0;

  // The count of metrics recording pauses.
  size_t num_metrics_pause_ = 0;

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
