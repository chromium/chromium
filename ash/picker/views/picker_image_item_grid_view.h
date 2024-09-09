// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_IMAGE_ITEM_GRID_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_IMAGE_ITEM_GRID_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view.h"

namespace ash {

class PickerImageItemView;

// Container view for the image items in a section. The image items are
// displayed in a grid with two columns.
class ASH_EXPORT PickerImageItemGridView
    : public views::View,
      public PickerTraversableItemContainer {
  METADATA_HEADER(PickerImageItemGridView, views::View)

 public:
  explicit PickerImageItemGridView(int grid_width);
  PickerImageItemGridView(const PickerImageItemGridView&) = delete;
  PickerImageItemGridView& operator=(const PickerImageItemGridView&) = delete;
  ~PickerImageItemGridView() override;

  // views::View:
  views::FocusTraversable* GetPaneFocusTraversable() override;

  // PickerTraversableItemContainer:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;
  bool ContainsItem(views::View* item) override;

  PickerImageItemView* AddImageItem(
      std::unique_ptr<PickerImageItemView> image_item);

 private:
  class FocusSearch : public views::FocusSearch,
                      public views::FocusTraversable {
   public:
    using GetFocusableViewsCallback =
        base::RepeatingCallback<const views::View::Views&(void)>;

    FocusSearch(views::View* view, const GetFocusableViewsCallback& callback);
    FocusSearch(const FocusSearch&) = delete;
    FocusSearch& operator=(const FocusSearch) = delete;
    ~FocusSearch() override;

    // views::FocusSearch:
    views::View* FindNextFocusableView(
        views::View* starting_view,
        SearchDirection search_direction,
        TraversalDirection traversal_direction,
        StartingViewPolicy check_starting_view,
        AnchoredDialogPolicy can_go_into_anchored_dialog,
        views::FocusTraversable** focus_traversable,
        views::View** focus_traversable_view) override;

    // views::FocusTraversable:
    views::FocusSearch* GetFocusSearch() override;
    views::FocusTraversable* GetFocusTraversableParent() override;
    views::View* GetFocusTraversableParentView() override;

   private:
    const raw_ptr<views::View> view_ = nullptr;
    const GetFocusableViewsCallback get_focusable_views_callback_;
  };

  // Returns the column containing `item`, or nullptr if `item` is not part of
  // this grid.
  views::View* GetColumnContaining(views::View* item);

  // Returns items in this grid in focus traversal order.
  const views::View::Views& GetFocusableItems() const;

  int grid_width_ = 0;
  views::View::Views focusable_items_;
  std::unique_ptr<FocusSearch> focus_search_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_IMAGE_ITEM_GRID_VIEW_H_
