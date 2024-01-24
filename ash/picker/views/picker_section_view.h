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
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class PickerItemView;

// View for a Picker section with a title and related items.
class ASH_EXPORT PickerSectionView : public views::View {
  METADATA_HEADER(PickerSectionView, views::View)

 public:
  explicit PickerSectionView(const std::u16string& title_text);
  PickerSectionView(const PickerSectionView&) = delete;
  PickerSectionView& operator=(const PickerSectionView&) = delete;
  ~PickerSectionView() override;

  // Sets the maximum width available for laying out section items.
  void SetMaximumWidth(int maximum_width);

  // Adds an item to the section.
  void AddItem(std::unique_ptr<PickerItemView> item_view);

  const views::Label* title_for_testing() const { return title_; }

  const views::View* small_grid_items_container_for_testing() const {
    return small_grid_items_container_;
  }
  const views::View* large_grid_items_container_for_testing() const {
    return large_grid_items_container_;
  }

  base::span<const raw_ptr<PickerItemView>> item_views_for_testing() const {
    return item_views_;
  }

 private:
  // Adds a small grid item. These are displayed in rows. If there may be more
  // than one row, `maximum_width_` should be set before adding small grid items
  // to ensure the rows are laid out correctly.
  void AddSmallGridItem(std::unique_ptr<PickerItemView> small_grid_item);

  // Adds a large grid item. These are displayed in columns.
  void AddLargeGridItem(std::unique_ptr<PickerItemView> large_grid_item);

  // Adds a list item. These are displayed in a vertical list, each item
  // spanning the width of the section.
  void AddListItem(std::unique_ptr<PickerItemView> list_item);

  // Maximum width available for laying out section items. If not set, we assume
  // the available width is unbounded during layout, so small grid items will be
  // laid out in a single row.
  std::optional<int> maximum_width_;

  raw_ptr<views::Label> title_ = nullptr;

  raw_ptr<views::View> small_grid_items_container_ = nullptr;
  raw_ptr<views::View> large_grid_items_container_ = nullptr;
  raw_ptr<views::View> list_items_container_ = nullptr;

  // The views for each result item.
  std::vector<raw_ptr<PickerItemView>> item_views_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SECTION_VIEW_H_
