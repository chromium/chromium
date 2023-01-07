// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_item.h"

namespace ash {

ClipboardHistoryItem::ClipboardHistoryItem(ui::ClipboardData data)
    : id_(base::UnguessableToken::Create()),
      data_(std::move(data)),
      time_copied_(base::Time::Now()) {}

ClipboardHistoryItem::ClipboardHistoryItem(const ClipboardHistoryItem&) =
    default;

ClipboardHistoryItem::ClipboardHistoryItem(ClipboardHistoryItem&&) = default;

ClipboardHistoryItem::~ClipboardHistoryItem() = default;

ui::ClipboardData ClipboardHistoryItem::ReplaceEquivalentData(
    ui::ClipboardData&& new_data) {
  DCHECK(data_ == new_data);
  time_copied_ = base::Time::Now();
  // If work has already been done to encode an image belonging to both data
  // instances, make sure it is not lost.
  if (data_.maybe_png() && !new_data.maybe_png())
    new_data.SetPngDataAfterEncoding(*data_.maybe_png());
  return std::exchange(data_, std::move(new_data));
}

}  // namespace ash
