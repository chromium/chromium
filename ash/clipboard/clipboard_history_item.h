// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_ITEM_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_ITEM_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "ui/base/clipboard/clipboard_data.h"

namespace ash {

// Wraps ClipboardData with a unique identifier.
class ASH_EXPORT ClipboardHistoryItem {
 public:
  explicit ClipboardHistoryItem(ui::ClipboardData data);
  ClipboardHistoryItem(const ClipboardHistoryItem&);
  ClipboardHistoryItem(ClipboardHistoryItem&&);

  // Copy assignment operator is deleted to be consistent with
  // `ui::ClipboardData`.
  ClipboardHistoryItem& operator=(const ClipboardHistoryItem&) = delete;

  ~ClipboardHistoryItem();

  // Replaces `data_` with `new_data`. The two data instances must be equal,
  // i.e., their contents (not including sequence number) must be the same.
  // Returns the replaced `data_`.
  ui::ClipboardData ReplaceEquivalentData(ui::ClipboardData&& new_data);

  const base::UnguessableToken& id() const { return id_; }
  const ui::ClipboardData& data() const { return data_; }
  const base::Time time_copied() const { return time_copied_; }

 private:
  // Unique identifier.
  const base::UnguessableToken id_;

  // Underlying data for an item in the clipboard history menu.
  ui::ClipboardData data_;

  // Time when the item's current data was set.
  base::Time time_copied_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_ITEM_H_
