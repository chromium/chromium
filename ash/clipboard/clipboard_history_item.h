// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_ITEM_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_ITEM_H_

#include <string>

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/models/image_model.h"

namespace ash {

// Wraps `ClipboardData` with extra metadata for the data's visual presentation.
class ASH_EXPORT ClipboardHistoryItem {
 public:
  // Note: `data` must have at least one supported format, as determined by
  // `clipboard_history_util::IsSupported()`.
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
  ui::ClipboardInternalFormat main_format() const { return main_format_; }
  crosapi::mojom::ClipboardHistoryDisplayFormat display_format() const {
    return display_format_;
  }
  void set_display_image(const ui::ImageModel& display_image) {
    DCHECK(display_image.IsImage());
    display_image_ = display_image;
  }
  const absl::optional<ui::ImageModel>& display_image() const {
    return display_image_;
  }
  const std::u16string& display_text() const { return display_text_; }
  const absl::optional<ui::ImageModel>& icon() const { return icon_; }

 private:
  // Unique identifier.
  const base::UnguessableToken id_;

  // Underlying data for an item in the clipboard history menu.
  ui::ClipboardData data_;

  // Time when the item's current data was set.
  base::Time time_copied_;

  // The most highly prioritized format present in `data_`, based on the
  // usefulness of that format's presentation to the user.
  const ui::ClipboardInternalFormat main_format_;

  // The item's categorization based on the options we have for presenting data
  // to the user.
  const crosapi::mojom::ClipboardHistoryDisplayFormat display_format_;

  // Cached display image. For PNG items, this will be set during construction.
  // For HTML items, this will be a placeholder image until the real preview is
  // ready, at which point it will be updated. For other items, there will be no
  // value.
  absl::optional<ui::ImageModel> display_image_;

  // The text that should be displayed on this item's menu entry.
  const std::u16string display_text_;

  // Cached image model for the item's icon. Currently, there will be no value
  // for non-file items.
  const absl::optional<ui::ImageModel> icon_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_ITEM_H_
