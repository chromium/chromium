// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SECTION_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SECTION_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/link.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class PickerEmojiItemView;
class PickerEmoticonItemView;
class PickerImageItemGridView;
class PickerImageItemView;
class PickerItemView;
class PickerListItemContainerView;
class PickerListItemView;
class PickerSmallItemGridView;
class PickerSymbolItemView;
class PickerTraversableItemContainer;

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
  PickerListItemView* AddListItem(
      std::unique_ptr<PickerListItemView> list_item);

  // Adds a emoji, symbol or emoticon. These are treated collectively as small
  // grid items and are displayed in rows.
  PickerEmojiItemView* AddEmojiItem(
      std::unique_ptr<PickerEmojiItemView> emoji_item);
  PickerSymbolItemView* AddSymbolItem(
      std::unique_ptr<PickerSymbolItemView> symbol_item);
  PickerEmoticonItemView* AddEmoticonItem(
      std::unique_ptr<PickerEmoticonItemView> emoticon_item);

  // Adds an image item to the section. These are displayed in a grid with two
  // columns.
  PickerImageItemView* AddImageItem(
      std::unique_ptr<PickerImageItemView> image_item);

  // Returns the item to highlight to when navigating to this section from the
  // top, or nullptr if the section is empty.
  PickerItemView* GetTopItem();

  // Returns the item to highlight to when navigating to this section from the
  // bottom, or nullptr if the section is empty.
  PickerItemView* GetBottomItem();

  // Returns the item directly above `item`, or nullptr if there is no such item
  // in the section.
  PickerItemView* GetItemAbove(PickerItemView* item);

  // Returns the item directly below `item`, or nullptr if there is no such item
  // in the section.
  PickerItemView* GetItemBelow(PickerItemView* item);

  // Returns the item directly to the left of `item`, or nullptr if there is no
  // such item in the section.
  PickerItemView* GetItemLeftOf(PickerItemView* item);

  // Returns the item directly to the right of `item`, or nullptr if there is no
  // such item in the section.
  PickerItemView* GetItemRightOf(PickerItemView* item);

  const views::Label* title_label_for_testing() const { return title_label_; }
  const views::Link* title_trailing_link_for_testing() const {
    return title_trailing_link_;
  }
  views::Link* title_trailing_link_for_testing() {
    return title_trailing_link_;
  }

  // TODO: b/322900302 - Figure out a nice way to access the item views for
  // keyboard navigation (e.g. how to handle grid items).
  base::span<const raw_ptr<PickerItemView>> item_views() const {
    return item_views_;
  }

  base::span<const raw_ptr<PickerItemView>> item_views_for_testing() const {
    return item_views_;
  }

 private:
  void CreateSmallItemGridIfNeeded();

  // Returns a non-null item container if the section has one, otherwise returns
  // nullptr.
  // TODO: b/322900302 - Determine whether sections can have multiple item
  // containers. If so, `GetItemContainer` will need to get the right item
  // container. If not, just track a single PickerTraversableItemContainer and
  // then we won't need this method anymore.
  PickerTraversableItemContainer* GetItemContainer();

  // Width available for laying out section items. This is needed to determine
  // row and column widths for grid items in the section.
  int section_width_ = 0;

  // Container for the section title contents, which can have a title label and
  // a trailing link.
  raw_ptr<views::View> title_container_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Link> title_trailing_link_ = nullptr;

  raw_ptr<PickerListItemContainerView> list_item_container_ = nullptr;
  raw_ptr<PickerSmallItemGridView> small_item_grid_ = nullptr;
  raw_ptr<PickerImageItemGridView> image_item_grid_ = nullptr;

  // The views for each result item.
  std::vector<raw_ptr<PickerItemView>> item_views_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SECTION_VIEW_H_
