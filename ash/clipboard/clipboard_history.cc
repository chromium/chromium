// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history.h"

#include <deque>

#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/scoped_clipboard_history_pause_impl.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/token.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace ash {

using PauseBehavior = clipboard_history_util::PauseBehavior;

ClipboardHistory::ClipboardHistory() {
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);
}

ClipboardHistory::~ClipboardHistory() {
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
}

void ClipboardHistory::AddObserver(Observer* observer) const {
  observers_.AddObserver(observer);
}

void ClipboardHistory::RemoveObserver(Observer* observer) const {
  observers_.RemoveObserver(observer);
}

const std::list<ClipboardHistoryItem>& ClipboardHistory::GetItems() const {
  return history_list_;
}

std::list<ClipboardHistoryItem>& ClipboardHistory::GetItems() {
  return history_list_;
}

void ClipboardHistory::Clear() {
  history_list_ = std::list<ClipboardHistoryItem>();
  SyncClipboardToClipboardHistory();
  for (auto& observer : observers_)
    observer.OnClipboardHistoryCleared();
}

bool ClipboardHistory::IsEmpty() const {
  return GetItems().empty();
}

void ClipboardHistory::RemoveItemForId(const base::UnguessableToken& id) {
  auto iter = base::ranges::find(history_list_, id, &ClipboardHistoryItem::id);

  // It is possible that the item specified by `id` has been removed. For
  // example, `history_list_` has reached its maximum capacity. while the
  // clipboard history menu is showing, a new item is added to `history_list_`.
  // Then the user wants to delete the item which has already been removed due
  // to overflow in `history_list_`.
  if (iter == history_list_.cend())
    return;

  auto removed = std::move(*iter);
  history_list_.erase(iter);
  SyncClipboardToClipboardHistory();
  for (auto& observer : observers_)
    observer.OnClipboardHistoryItemRemoved(removed);
}

void ClipboardHistory::OnClipboardDataChanged() {
  if (!clipboard_history_util::IsEnabledInCurrentMode())
    return;

  if (!pauses_.empty() &&
      pauses_.front().pause_behavior == PauseBehavior::kDefault) {
    return;
  }

  // The clipboard may not exist in tests.
  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  if (!clipboard)
    return;

  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  const auto* clipboard_data = clipboard->GetClipboardData(&data_dst);
  if (!clipboard_data) {
    // `clipboard_data` is only empty when the clipboard is cleared. This is
    // done to prevent data leakage into or from locked states (e.g., locked
    // fullscreen). Clipboard history should also be cleared in this case.
    commit_data_weak_factory_.InvalidateWeakPtrs();
    Clear();
    return;
  }

  // We post a task to commit `clipboard_data` at the end of the current task
  // sequence to debounce the case where multiple copies are programmatically
  // performed. Since only the most recent copy will be at the top of the
  // clipboard, the user will likely be unaware of the intermediate copies that
  // took place opaquely in the same task sequence and would be confused to see
  // them in history. A real-world example would be copying the URL from the
  // address bar in the browser. First a short form of the URL is copied,
  // followed immediately by the long-form URL.
  commit_data_weak_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClipboardHistory::MaybeCommitData,
                     commit_data_weak_factory_.GetWeakPtr(), *clipboard_data,
                     /*is_reorder_on_paste=*/!pauses_.empty() &&
                         pauses_.front().pause_behavior ==
                             PauseBehavior::kAllowReorderOnPaste));

  // If clipboard history was paused with a contingency that allowed data to be
  // committed, the operation that changed clipboard data was not a user's copy.
  if (pauses_.empty()) {
    // Debounce calls to `OnClipboardOperation()`. Certain surfaces
    // (Omnibox) may read/write to the clipboard multiple times in one
    // user-initiated operation. Add a delay because `PostTask()` is too fast to
    // debounce multiple operations through the async web clipboard API. See
    // https://crbug.com/1167403.
    clipboard_histogram_weak_factory_.InvalidateWeakPtrs();
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ClipboardHistory::OnClipboardOperation,
                       clipboard_histogram_weak_factory_.GetWeakPtr(),
                       /*copy=*/true),
        base::Milliseconds(100));
  }
}

void ClipboardHistory::OnClipboardDataRead() {
  if (!pauses_.empty())
    return;

  // Debounce calls to `OnClipboardOperation()`. Certain surfaces
  // (Omnibox) may read/write to the clipboard multiple times in one
  // user-initiated operation. Add a delay because `PostTask()` is too fast to
  // debounce multiple operations through the async web clipboard API. See
  // https://crbug.com/1167403.
  clipboard_histogram_weak_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ClipboardHistory::OnClipboardOperation,
                     clipboard_histogram_weak_factory_.GetWeakPtr(),
                     /*copy=*/false),
      base::Milliseconds(100));
}

void ClipboardHistory::OnClipboardOperation(bool copy) {
  for (auto& observer : observers_)
    observer.OnOperationConfirmed(copy);

  using Operation = clipboard_history_util::Operation;
  base::UmaHistogramEnumeration("Ash.ClipboardHistory.Operation",
                                copy ? Operation::kCopy : Operation::kPaste);

  if (copy) {
    consecutive_copies_++;
    if (consecutive_pastes_ > 0) {
      base::UmaHistogramCounts100("Ash.Clipboard.ConsecutivePastes",
                                  consecutive_pastes_);
      consecutive_pastes_ = 0;
    }
  } else {
    // Note: This includes pastes by the clipboard history menu.
    consecutive_pastes_++;
    if (consecutive_copies_ > 0) {
      base::UmaHistogramCounts100("Ash.Clipboard.ConsecutiveCopies",
                                  consecutive_copies_);
      consecutive_copies_ = 0;
    }
  }
}

base::WeakPtr<ClipboardHistory> ClipboardHistory::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ClipboardHistory::SyncClipboardToClipboardHistory() {
  // The clipboard may not exist in tests.
  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  if (!clipboard)
    return;

  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  const auto* clipboard_data = clipboard->GetClipboardData(&data_dst);

  // Only modify the clipboard if doing so would change its data, so as to avoid
  // extraneous notifications to clipboard observers. If there is a change to
  // make, pause clipboard history so that making the clipboard consistent with
  // clipboard history does not cause clipboard history to update again.
  ScopedClipboardHistoryPauseImpl scoped_pause(this);
  if (history_list_.empty()) {
    if (clipboard_data) {
      static_cast<ui::Clipboard*>(clipboard)->Clear(
          ui::ClipboardBuffer::kCopyPaste);
    }
  } else if (const auto& top_of_history_data = history_list_.front().data();
             top_of_history_data != *clipboard_data) {
    clipboard->WriteClipboardData(
        std::make_unique<ui::ClipboardData>(top_of_history_data));
  }
}

void ClipboardHistory::MaybeCommitData(ui::ClipboardData data,
                                       bool is_reorder_on_paste) {
  if (!clipboard_history_util::IsSupported(data))
    return;

  auto iter =
      base::ranges::find(history_list_, data, &ClipboardHistoryItem::data);
  bool is_duplicate = iter != history_list_.cend();
  if (is_duplicate) {
    // If `data` already exists in `history_list_` then move its corresponding
    // item to the front of the list instead of creating a new item, because
    // creating a new item will result in a new unique identifier. Replace the
    // item's underlying clipboard data for consistency with the clipboard's
    // current state.
    iter->ReplaceEquivalentData(std::move(data));
    history_list_.splice(history_list_.begin(), history_list_, iter);
    using ReorderType = clipboard_history_util::ReorderType;
    base::UmaHistogramEnumeration(
        "Ash.ClipboardHistory.ReorderType",
        is_reorder_on_paste ? ReorderType::kOnPaste : ReorderType::kOnCopy);
  } else {
    DCHECK(!is_reorder_on_paste);
    history_list_.emplace_front(std::move(data));
  }

  for (auto& observer : observers_)
    observer.OnClipboardHistoryItemAdded(history_list_.front(), is_duplicate);

  if (history_list_.size() > clipboard_history_util::kMaxClipboardItems) {
    auto removed = std::move(history_list_.back());
    history_list_.pop_back();
    for (auto& observer : observers_)
      observer.OnClipboardHistoryItemRemoved(removed);
  }
}

const base::Token& ClipboardHistory::Pause(PauseBehavior pause_behavior) {
  pauses_.push_front({base::Token::CreateRandom(), pause_behavior});
  return pauses_.front().pause_id;
}

void ClipboardHistory::Resume(const base::Token& pause_id) {
  auto pause_it = base::ranges::find(pauses_, pause_id, &PauseInfo::pause_id);
  DCHECK(pause_it != pauses_.end());
  pauses_.erase(pause_it);
}

}  // namespace ash
