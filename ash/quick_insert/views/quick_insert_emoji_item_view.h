// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_EMOJI_ITEM_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_EMOJI_ITEM_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

// Quick Insert item which contains just an emoji.
class ASH_EXPORT PickerEmojiItemView : public QuickInsertItemView {
  METADATA_HEADER(PickerEmojiItemView, QuickInsertItemView)

 public:
  enum class Style { kEmoji, kEmoticon, kSymbol };

  PickerEmojiItemView(Style style,
                      SelectItemCallback select_item_callback,
                      const std::u16string& text);
  PickerEmojiItemView(const PickerEmojiItemView&) = delete;
  PickerEmojiItemView& operator=(const PickerEmojiItemView&) = delete;
  ~PickerEmojiItemView() override;

  std::u16string_view GetTextForTesting() const;

 private:
  raw_ptr<views::Label> label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_EMOJI_ITEM_VIEW_H_
