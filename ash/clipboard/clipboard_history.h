// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_

#include <list>
#include <map>

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell_observer.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_observer.h"

namespace ash {

// Keeps track of the last few things saved in the clipboard.
class ASH_EXPORT ClipboardHistory : public ui::ClipboardObserver {
 public:
  class ASH_EXPORT Observer : public base::CheckedObserver {
   public:
    // Called when a ClipboardHistoryItem has been added.
    virtual void OnClipboardHistoryItemAdded(const ClipboardHistoryItem& item) {
    }
    // Called when a ClipboardHistoryItem has been removed.
    virtual void OnClipboardHistoryItemRemoved(
        const ClipboardHistoryItem& item) {}
    // Called when ClipboardHistory is Clear()-ed.
    virtual void OnClipboardHistoryCleared() {}
  };

  // Prevents clipboard history from being recorded within its scope. If
  // anything is copied within its scope, history will not be recorded.
  class ASH_EXPORT ScopedPause {
   public:
    explicit ScopedPause(ClipboardHistory* clipboard_history);
    ScopedPause(const ScopedPause&) = delete;
    ScopedPause& operator=(const ScopedPause&) = delete;
    ~ScopedPause();

   private:
    ClipboardHistory* const clipboard_history_;
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

  // ClipboardMonitor:
  void OnClipboardDataChanged() override;

  // Returns whether the clipboard history is enabled for the current user mode.
  bool IsEnabledInCurrentMode() const;

 private:
  // Adds `data` to the `history_list_` if it's supported. If `data` is not
  // supported by clipboard history, this method no-ops.
  void MaybeCommitData(ui::ClipboardData data);

  void Pause();
  void Resume();

  // The count of pauses.
  size_t num_pause_ = 0;

  // The history of data copied to the Clipboard. Items of the list are sorted
  // by recency.
  std::list<ClipboardHistoryItem> history_list_;

  // Mutable to allow adding/removing from |observers_| through a const
  // ClipboardHistory.
  mutable base::ObserverList<Observer> observers_;

  // Factory to create WeakPtrs used to debounce calls to CommitData().
  base::WeakPtrFactory<ClipboardHistory> commit_data_weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_
