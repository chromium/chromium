// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history.h"

#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/clipboard_nudge_controller.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace ash {

namespace {

// The different operations ClipboardHistory sees. These values are written to
// logs. New enum values can be added, but existing enums must never be
// renumbered, deleted, or reused. Keep this up to date with the
// ClipboardHistoryOperation enum in enums.xml.
enum class ClipboardHistoryOperation {
  // Copy, initiated through any method which triggers the clipboard to be
  // written to.
  kCopy = 0,

  // Paste, detected when the clipboard is read.
  kPaste = 1,

  // Insert new types above this line.
  kMaxValue = kPaste
};

void RecordClipboardHistoryOperation(ClipboardHistoryOperation operation) {
  base::UmaHistogramEnumeration("Ash.ClipboardHistory.Operation", operation);
}

}  // namespace

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

void ClipboardHistory::Clear() {
  history_list_ = std::list<ClipboardHistoryItem>();
  for (auto& observer : observers_)
    observer.OnClipboardHistoryCleared();
}

bool ClipboardHistory::IsEmpty() const {
  return GetItems().empty();
}

void ClipboardHistory::RemoveItemForId(const base::UnguessableToken& id) {
  auto iter = std::find_if(history_list_.cbegin(), history_list_.cend(),
                           [&id](const auto& item) { return item.id() == id; });

  // It is possible that the item specified by `id` has been removed. For
  // example, `history_list_` has reached its maximum capacity. while the
  // clipboard history menu is showing, a new item is added to `history_list_`.
  // Then the user wants to delete the item which has already been removed due
  // to overflow in `history_list_`.
  if (iter == history_list_.cend())
    return;

  auto removed = std::move(*iter);
  history_list_.erase(iter);
  for (auto& observer : observers_)
    observer.OnClipboardHistoryItemRemoved(removed);
}

void ClipboardHistory::OnClipboardDataChanged() {
  if (!ClipboardHistoryUtil::IsEnabledInCurrentMode())
    return;

  if (num_pause_ > 0)
    return;

  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  // Clipboard may not exist in tests.
  if (!clipboard)
    return;

  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  const auto* clipboard_data = clipboard->GetClipboardData(&data_dst);
  if (!clipboard_data) {
    // |clipboard_data| is only empty when the Clipboard is cleared. This is
    // done to prevent data leakage into or from locked forms(Locked Fullscreen
    // state). Clear ClipboardHistory.
    commit_data_weak_factory_.InvalidateWeakPtrs();
    Clear();
    return;
  }

  // Debounce calls to `OnClipboardOperation()`. Certain surfaces
  // (Omnibox) may Read/Write to the clipboard multiple times in one user
  // initiated operation. Add a delay because PostTask is too fast to debounce
  // multiple operations through the async web clipboard API. See
  // https://crbug.com/1167403.
  clipboard_histogram_weak_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ClipboardHistory::OnClipboardOperation,
                     clipboard_histogram_weak_factory_.GetWeakPtr(),
                     /*copy=*/true),
      base::Milliseconds(100));

  // We post commit |clipboard_data| at the end of the current task sequence to
  // debounce the case where multiple copies are programmatically performed.
  // Since only the most recent copy will be at the top of the clipboard, the
  // user will likely be unaware of the intermediate copies that took place
  // opaquely in the same task sequence and would be confused to see them in
  // history. A real world example would be copying the URL from the address bar
  // in the browser. First a short form of the URL is copied, followed
  // immediately by the long form URL.
  commit_data_weak_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClipboardHistory::MaybeCommitData,
                     commit_data_weak_factory_.GetWeakPtr(), *clipboard_data));
}

void ClipboardHistory::OnClipboardDataRead() {
  if (num_pause_ > 0)
    return;

  // Debounce calls to `OnClipboardOperation()`. Certain surfaces
  // (Omnibox) may Read/Write to the clipboard multiple times in one user
  // initiated operation. Add a delay because PostTask is too fast to debounce
  // multiple operations through the async web clipboard API. See
  // https://crbug.com/1167403.
  clipboard_histogram_weak_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ClipboardHistory::OnClipboardOperation,
                     clipboard_histogram_weak_factory_.GetWeakPtr(),
                     /*copy=*/false),
      base::Milliseconds(100));
}

void ClipboardHistory::OnClipboardOperation(bool copy) {
  for (auto& observer : observers_)
    observer.OnOperationConfirmed(copy);

  if (copy) {
    RecordClipboardHistoryOperation(ClipboardHistoryOperation::kCopy);
    consecutive_copies_++;
    if (consecutive_pastes_ > 0) {
      base::UmaHistogramCounts100("Ash.Clipboard.ConsecutivePastes",
                                  consecutive_pastes_);
      consecutive_pastes_ = 0;
    }
    return;
  }

  consecutive_pastes_++;
  // NOTE: this includes pastes by the ClipboardHistory menu.

  RecordClipboardHistoryOperation(ClipboardHistoryOperation::kPaste);
  if (consecutive_copies_ > 0) {
    base::UmaHistogramCounts100("Ash.Clipboard.ConsecutiveCopies",
                                consecutive_copies_);
    consecutive_copies_ = 0;
  }
}

base::WeakPtr<ClipboardHistory> ClipboardHistory::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ClipboardHistory::MaybeCommitData(ui::ClipboardData data) {
  if (!ClipboardHistoryUtil::IsSupported(data))
    return;

  auto iter =
      std::find_if(history_list_.cbegin(), history_list_.cend(),
                   [&data](const auto& item) { return item.data() == data; });
  bool is_duplicate = iter != history_list_.cend();
  if (is_duplicate) {
    // If |data| already exists in |history_list_| then move it to the front
    // instead of creating a new one because creating a new one will result in a
    // new unique identifier.
    history_list_.splice(history_list_.begin(), history_list_, iter);
  } else {
    history_list_.emplace_front(std::move(data));
  }

  for (auto& observer : observers_)
    observer.OnClipboardHistoryItemAdded(history_list_.front(), is_duplicate);

  if (history_list_.size() > ClipboardHistoryUtil::kMaxClipboardItemsShared) {
    auto removed = std::move(history_list_.back());
    history_list_.pop_back();
    for (auto& observer : observers_)
      observer.OnClipboardHistoryItemRemoved(removed);
  }
}

void ClipboardHistory::Pause() {
  ++num_pause_;
}

void ClipboardHistory::Resume() {
  --num_pause_;
}

}  // namespace ash
