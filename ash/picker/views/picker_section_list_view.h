// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SECTION_LIST_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SECTION_LIST_VIEW_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PickerItemView;
class PickerSectionView;

// View which displays Picker sections in a vertical list.
class ASH_EXPORT PickerSectionListView : public views::View {
  METADATA_HEADER(PickerSectionListView, views::View)

 public:
  explicit PickerSectionListView(int section_width);
  PickerSectionListView(const PickerSectionListView&) = delete;
  PickerSectionListView& operator=(const PickerSectionListView&) = delete;
  ~PickerSectionListView() override;

  // Returns the item to highlight to when navigating to this section list from
  // the top, or nullptr if the section list is empty.
  PickerItemView* GetTopItem();

  // Returns the item to highlight to when navigating to this section list from
  // the bottom, or nullptr if the section list is empty.
  PickerItemView* GetBottomItem();

  // Returns the item directly above `item`, or nullptr if there is no such item
  // in the section list.
  PickerItemView* GetItemAbove(PickerItemView* item);

  // Returns the item directly below `item`, or nullptr if there is no such item
  // in the section list.
  PickerItemView* GetItemBelow(PickerItemView* item);

  // Returns the item directly to the left of `item`, or nullptr if there is no
  // such item in the section list.
  PickerItemView* GetItemLeftOf(PickerItemView* item);

  // Returns the item directly to the right of `item`, or nullptr if there is no
  // such item in the section list.
  PickerItemView* GetItemRightOf(PickerItemView* item);

  // Adds a section to the end of the section list.
  PickerSectionView* AddSection();

  // Adds a section to the specified position in the list.
  PickerSectionView* AddSectionAt(size_t index);

  // Clears the section list. This deletes all contained sections and items.
  void ClearSectionList();

 private:
  // Returns the section containing `item`, or nullptr if `item` is not part of
  // this section list.
  PickerSectionView* GetSectionContaining(PickerItemView* item);

  // Width of the sections in this view.
  int section_width_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SECTION_LIST_VIEW_H_
