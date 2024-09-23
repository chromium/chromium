// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SECTION_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SECTION_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/picker_search_result.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/link.h"
#include "ui/views/view.h"

namespace views {
class BoxLayoutView;
class Label;
}  // namespace views

namespace ash {

class PickerAssetFetcher;
class PickerImageItemGridView;
class PickerImageItemRowView;
class PickerImageItemView;
class PickerItemWithSubmenuView;
class PickerItemView;
class PickerListItemContainerView;
class PickerListItemView;
class PickerPreviewBubbleController;
class PickerSubmenuController;
class PickerTraversableItemContainer;
enum class PickerActionType;

// View for a Picker section with a title and related items.
class ASH_EXPORT PickerSectionView : public views::View {
  METADATA_HEADER(PickerSectionView, views::View)

 public:
  using SelectResultCallback = base::RepeatingClosure;

  // Describes the way local file results are visually presented.
  enum class LocalFileResultStyle {
    // Shown as list items with the name of the file as the label.
    kList,
    // Shown as a grid of thumbnail previews.
    kGrid,
    // Shown as a single row of thumbnail previews.
    kRow,
  };

  explicit PickerSectionView(int section_width,
                             PickerAssetFetcher* asset_fetcher,
                             PickerSubmenuController* submenu_controller);
  PickerSectionView(const PickerSectionView&) = delete;
  PickerSectionView& operator=(const PickerSectionView&) = delete;
  ~PickerSectionView() override;

  // Creates an item based on `result` and adds it to the section view.
  // `preview_controller` can be null if previews are not needed.
  // `asset_fetcher` can be null for most result types.
  // Both `preview_controller` and `asset_fetcher` must outlive the return
  // value.
  static std::unique_ptr<PickerItemView> CreateItemFromResult(
      const PickerSearchResult& result,
      PickerPreviewBubbleController* preview_controller,
      PickerAssetFetcher* asset_fetcher,
      int available_width,
      LocalFileResultStyle local_file_result_style,
      SelectResultCallback select_result_callback);

  void AddTitleLabel(const std::u16string& title_text);
  void AddTitleTrailingLink(const std::u16string& link_text,
                            const std::u16string& accessible_name,
                            views::Link::ClickedCallback link_callback);

  // Adds a list item. These are displayed in a vertical list, each item
  // spanning the width of the section.
  PickerListItemView* AddListItem(
      std::unique_ptr<PickerListItemView> list_item);

  // Adds an image item to the section. These are displayed in a grid with two
  // columns.
  PickerImageItemView* AddImageGridItem(
      std::unique_ptr<PickerImageItemView> image_item);

  // Adds an image item to the section. These are displayed in a single row.
  PickerImageItemView* AddImageRowItem(
      std::unique_ptr<PickerImageItemView> image_item);

  // Adds an item with submenu to the section.
  PickerItemWithSubmenuView* AddItemWithSubmenu(
      std::unique_ptr<PickerItemWithSubmenuView> item_with_submenu);

  // Same as `CreateItemFromResult`, but additionally adds the item to this
  // section.
  PickerItemView* AddResult(const PickerSearchResult& result,
                            PickerPreviewBubbleController* preview_controller,
                            LocalFileResultStyle local_file_result_style,
                            SelectResultCallback select_result_callback);

  void ClearItems();

  // Returns the item to highlight to when navigating to this section from the
  // top, or nullptr if the section is empty.
  views::View* GetTopItem();

  // Returns the item to highlight to when navigating to this section from the
  // bottom, or nullptr if the section is empty.
  views::View* GetBottomItem();

  // Returns the item directly above `item`, or nullptr if there is no such item
  // in the section.
  views::View* GetItemAbove(views::View* item);

  // Returns the item directly below `item`, or nullptr if there is no such item
  // in the section.
  views::View* GetItemBelow(views::View* item);

  // Returns the item directly to the left of `item`, or nullptr if there is no
  // such item in the section.
  views::View* GetItemLeftOf(views::View* item);

  // Returns the item directly to the right of `item`, or nullptr if there is no
  // such item in the section.
  views::View* GetItemRightOf(views::View* item);

  // Must be called before creating an image row.
  // `accessible_name` is the accessible name of the image row.
  void SetImageRowProperties(std::u16string accessible_name,
                             base::RepeatingClosure more_items_button_callback,
                             std::u16string more_items_button_accessible_name);

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
  views::View* GetImageRowMoreItemsButtonForTesting();

 private:
  struct ImageRowProperties {
    std::u16string accessible_name;
    base::RepeatingClosure more_items_button_callback;
    std::u16string more_items_button_accessible_name;

    ImageRowProperties();
    ~ImageRowProperties();
  };

  PickerListItemContainerView* GetOrCreateListItemContainer();
  PickerImageItemGridView* GetOrCreateImageItemGrid();
  PickerImageItemRowView* GetOrCreateImageItemRow();

  // Width available for laying out section items. This is needed to determine
  // row and column widths for grid items in the section.
  int section_width_ = 0;

  // Container for the section title contents, which can have a title label and
  // a trailing link.
  raw_ptr<views::BoxLayoutView> title_container_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Link> title_trailing_link_ = nullptr;

  std::vector<raw_ptr<PickerTraversableItemContainer>> item_containers_;
  raw_ptr<PickerListItemContainerView> list_item_container_ = nullptr;
  raw_ptr<PickerImageItemGridView> image_item_grid_ = nullptr;
  raw_ptr<PickerImageItemRowView> image_item_row_ = nullptr;

  // The views for each result item.
  std::vector<raw_ptr<PickerItemView>> item_views_;

  // `asset_fetcher` outlives `this`.
  raw_ptr<PickerAssetFetcher> asset_fetcher_ = nullptr;

  // `submenu_controller` outlives `this`.
  raw_ptr<PickerSubmenuController> submenu_controller_ = nullptr;

  ImageRowProperties image_row_properties_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SECTION_VIEW_H_
