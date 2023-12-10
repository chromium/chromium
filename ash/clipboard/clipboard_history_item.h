// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_ITEM_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_ITEM_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/text_constants.h"

namespace ash {

// Wraps `ClipboardData` with extra metadata for the data's visual presentation.
class ASH_EXPORT ClipboardHistoryItem {
 public:
  // Note: `data` must have at least one supported format, as determined by
  // `clipboard_history_util::IsSupported()`.
  explicit ClipboardHistoryItem(ui::ClipboardData data);

  // TODO(b/279797913): Remove copy and move constructors once the clipboard
  // history model starts storing and returning shared pointers to items.
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

  // Sets `display_image_` to `display_image` and notifies
  // `display_image_updated_callbacks_`.
  void SetDisplayImage(const ui::ImageModel& display_image);

  // Adds `callback` to be notified if `display_image_` is updated.
  // Note: `callback` will only run if `display_image_` is updated on the item
  // to which it was added; copy-constructed and move-constructed items do not
  // retain callbacks added to the original.
  base::CallbackListSubscription AddDisplayImageUpdatedCallback(
      base::RepeatingClosure callback) const;

  const base::UnguessableToken& id() const { return id_; }
  const ui::ClipboardData& data() const { return data_; }
  const base::Time time_copied() const { return time_copied_; }
  ui::ClipboardInternalFormat main_format() const { return main_format_; }
  crosapi::mojom::ClipboardHistoryDisplayFormat display_format() const {
    return display_format_;
  }
  const std::optional<ui::ImageModel>& display_image() const {
    return display_image_;
  }
  const std::u16string& display_text() const { return display_text_; }
  const std::optional<gfx::ElideBehavior>& display_text_elide_behavior() const {
    return display_text_elide_behavior_;
  }
  const std::optional<size_t>& display_text_max_lines() const {
    return display_text_max_lines_;
  }
  size_t file_count() const { return file_count_; }
  const std::optional<ui::ImageModel>& icon() const { return icon_; }
  const std::optional<std::u16string>& secondary_display_text() const {
    return secondary_display_text_;
  }
  void set_secondary_display_text(
      const std::optional<std::u16string>& secondary_display_text) {
    secondary_display_text_ = secondary_display_text;
  }

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
  std::optional<ui::ImageModel> display_image_;

  // The text that should be displayed on this item's menu entry.
  const std::u16string display_text_;

  // TODO(http://b/275629173): Consider a new display format for URLs instead.
  // If present, overrides elide behavior for the text that should be displayed
  // on this item's menu entry.
  const std::optional<gfx::ElideBehavior> display_text_elide_behavior_;

  // TODO(http://b/275629173): Consider a new display format for URLs instead.
  // If present, overrides max lines for the text that should be displayed on
  // this item's menu entry.
  const std::optional<size_t> display_text_max_lines_;

  // Indicates the count of copied files in the underlying clipboard data.
  const size_t file_count_;

  // Cached image model for the item's icon. Currently, there will be no value
  // for non-file items.
  const std::optional<ui::ImageModel> icon_;

  // The text, if any, that should be displayed underneath `display_text_` on
  // this item's menu entry.
  std::optional<std::u16string> secondary_display_text_;

  // Mutable to allow const access from `AddDisplayImageUpdatedCallback()`.
  mutable base::RepeatingClosureList display_image_updated_callbacks_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_ITEM_H_
