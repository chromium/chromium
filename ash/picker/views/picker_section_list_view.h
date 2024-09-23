// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SECTION_LIST_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SECTION_LIST_VIEW_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PickerAssetFetcher;
class PickerSectionView;
class PickerSubmenuController;

// View which displays Picker sections in a vertical list.
class ASH_EXPORT PickerSectionListView : public views::View {
  METADATA_HEADER(PickerSectionListView, views::View)

 public:
  explicit PickerSectionListView(int section_width,
                                 PickerAssetFetcher* asset_fetcher,
                                 PickerSubmenuController* submenu_controller);
  PickerSectionListView(const PickerSectionListView&) = delete;
  PickerSectionListView& operator=(const PickerSectionListView&) = delete;
  ~PickerSectionListView() override;

  // Returns the item to highlight to when navigating to this section list from
  // the top, or nullptr if the section list is empty.
  views::View* GetTopItem();

  // Returns the item to highlight to when navigating to this section list from
  // the bottom, or nullptr if the section list is empty.
  views::View* GetBottomItem();

  // Returns the item directly above `item`, or nullptr if there is no such item
  // in the section list.
  views::View* GetItemAbove(views::View* item);

  // Returns the item directly below `item`, or nullptr if there is no such item
  // in the section list.
  views::View* GetItemBelow(views::View* item);

  // Returns the item directly to the left of `item`, or nullptr if there is no
  // such item in the section list.
  views::View* GetItemLeftOf(views::View* item);

  // Returns the item directly to the right of `item`, or nullptr if there is no
  // such item in the section list.
  views::View* GetItemRightOf(views::View* item);

  // Adds a section to the end of the section list.
  PickerSectionView* AddSection();

  // Adds a section to the specified position in the list.
  PickerSectionView* AddSectionAt(size_t index);

  // Clears the section list. This deletes all contained sections and items.
  void ClearSectionList();

 private:
  // Returns the section containing `item`, or nullptr if `item` is not part of
  // this section list.
  PickerSectionView* GetSectionContaining(views::View* item);

  // Width of the sections in this view.
  int section_width_;

  // `asset_fetcher` outlives `this`.
  raw_ptr<PickerAssetFetcher> asset_fetcher_ = nullptr;

  // `submenu_controller_` outlives `this`.
  raw_ptr<PickerSubmenuController> submenu_controller_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SECTION_LIST_VIEW_H_
