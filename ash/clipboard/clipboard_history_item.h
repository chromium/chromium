// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_ITEM_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_ITEM_H_

#include "ash/ash_export.h"
#include "base/unguessable_token.h"
#include "ui/base/clipboard/clipboard_data.h"

namespace ash {

// Wraps ClipboardData with a unique identifier.
class ASH_EXPORT ClipboardHistoryItem {
 public:
  explicit ClipboardHistoryItem(ui::ClipboardData data);
  ClipboardHistoryItem(const ClipboardHistoryItem&);
  ClipboardHistoryItem(ClipboardHistoryItem&&);

  // Copy/move assignment operators are deleted to be consistent with
  // ui::ClipboardData and ui::DataTransferEndpoint.
  ClipboardHistoryItem& operator=(const ClipboardHistoryItem&) = delete;
  ClipboardHistoryItem& operator=(ClipboardHistoryItem&&) = delete;

  ~ClipboardHistoryItem();

  const base::UnguessableToken& id() const { return id_; }
  const ui::ClipboardData& data() const { return data_; }

 private:
  // Unique identifier.
  base::UnguessableToken id_;
  ui::ClipboardData data_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_ITEM_H_
