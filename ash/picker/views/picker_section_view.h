// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SECTION_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SECTION_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
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

  void AddLargeGridItem(std::unique_ptr<PickerItemView> large_grid_item);
  void AddListItem(std::unique_ptr<PickerItemView> list_item);

  const views::Label* title_for_testing() const { return title_; }

  const views::View* large_grid_items_container_for_testing() const {
    return large_grid_items_container_;
  }

  base::span<const raw_ptr<PickerItemView>> item_views_for_testing() const {
    return item_views_;
  }

 private:
  raw_ptr<views::Label> title_ = nullptr;

  raw_ptr<views::View> large_grid_items_container_ = nullptr;
  raw_ptr<views::View> list_items_container_ = nullptr;

  // The views for each result item.
  std::vector<raw_ptr<PickerItemView>> item_views_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SECTION_VIEW_H_
