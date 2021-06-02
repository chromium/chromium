// Copyright 2020 The Chromium Authors. All rights reserved.
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

}  // namespace ash
