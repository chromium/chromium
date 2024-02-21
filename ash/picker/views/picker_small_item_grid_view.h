// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SMALL_ITEM_GRID_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SMALL_ITEM_GRID_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PickerItemView;
class PickerEmojiItemView;
class PickerSymbolItemView;
class PickerEmoticonItemView;

// Container view for the small items in a section, which can include emoji,
// symbol and emoticon items. These are displayed in a grid of rows.
class ASH_EXPORT PickerSmallItemGridView : public views::View {
  METADATA_HEADER(PickerSmallItemGridView, views::View)

 public:
  explicit PickerSmallItemGridView(int grid_width);
  PickerSmallItemGridView(const PickerSmallItemGridView&) = delete;
  PickerSmallItemGridView& operator=(const PickerSmallItemGridView&) = delete;
  ~PickerSmallItemGridView() override;

  PickerEmojiItemView* AddEmojiItem(
      std::unique_ptr<PickerEmojiItemView> emoji_item);
  PickerSymbolItemView* AddSymbolItem(
      std::unique_ptr<PickerSymbolItemView> symbol_item);
  PickerEmoticonItemView* AddEmoticonItem(
      std::unique_ptr<PickerEmoticonItemView> emoticon_item);

 private:
  PickerItemView* AddSmallGridItem(
      std::unique_ptr<PickerItemView> small_grid_item);

  int grid_width_ = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SMALL_ITEM_GRID_VIEW_H_
