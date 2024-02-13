// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SECTION_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SECTION_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/link.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class PickerEmojiItemView;
class PickerSymbolItemView;
class PickerEmoticonItemView;
class PickerImageItemView;

// View for a Picker section with a title and related items.
class ASH_EXPORT PickerSectionView : public views::View {
  METADATA_HEADER(PickerSectionView, views::View)

 public:
  explicit PickerSectionView(int section_width);
  PickerSectionView(const PickerSectionView&) = delete;
  PickerSectionView& operator=(const PickerSectionView&) = delete;
  ~PickerSectionView() override;

  void AddTitleLabel(const std::u16string& title_text);
  void AddTitleTrailingLink(const std::u16string& link_text,
                            views::Link::ClickedCallback link_callback);

  // Adds a list item. These are displayed in a vertical list, each item
  // spanning the width of the section.
  void AddListItem(std::unique_ptr<views::View> list_item);

  // Adds a emoji, symbol or emoticon. These are treated collectively as small
  // grid items and are displayed in rows.
  void AddEmojiItem(std::unique_ptr<PickerEmojiItemView> emoji_item);
  void AddSymbolItem(std::unique_ptr<PickerSymbolItemView> symbol_item);
  void AddEmoticonItem(std::unique_ptr<PickerEmoticonItemView> emoticon_item);

  // Adds an image item to the section. These are displayed in a grid with two
  // columns.
  void AddImageItem(std::unique_ptr<PickerImageItemView> image_item);

  const views::Label* title_label_for_testing() const { return title_label_; }

  const views::View* small_items_grid_for_testing() const {
    return small_items_grid_;
  }

  const views::View* image_grid_for_testing() const { return image_grid_; }

  base::span<const raw_ptr<views::View>> item_views_for_testing() const {
    return item_views_;
  }

 private:
  // Adds a small grid item. These are displayed in rows.
  void AddSmallGridItem(std::unique_ptr<views::View> small_grid_item);

  // Width available for laying out section items. This is needed to determine
  // row and column widths for grid items in the section.
  int section_width_ = 0;

  // Container for the section title contents, which can have a title label and
  // a trailing link.
  raw_ptr<views::View> title_container_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Link> title_trailing_link_ = nullptr;

  raw_ptr<views::Label> title_ = nullptr;

  raw_ptr<views::View> list_items_container_ = nullptr;

  raw_ptr<views::View> small_items_grid_ = nullptr;

  raw_ptr<views::View> image_grid_ = nullptr;

  // The views for each result item.
  std::vector<raw_ptr<views::View>> item_views_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SECTION_VIEW_H_
