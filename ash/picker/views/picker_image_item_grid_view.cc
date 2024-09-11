// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_image_item_grid_view.h"

#include <iterator>
#include <memory>
#include <utility>

#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "base/notimplemented.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

// Padding between image grid items.
constexpr int kImageGridPadding = 8;

// Margin for the image grid.
constexpr auto kImageGridMargin = gfx::Insets::TLBR(16, 16, 0, 16);

std::unique_ptr<views::View> CreateImageGridColumn() {
  auto column =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch)
          .Build();
  column->SetBetweenChildSpacing(kImageGridPadding);
  return column;
}

views::View* ItemInColumnWithIndexClosestTo(views::View* column,
                                            const size_t index) {
  if (column->children().empty()) {
    return nullptr;
  } else if (index < column->children().size()) {
    return column->children()[index]->children().front().get();
  } else {
    return column->children().back()->children().front().get();
  }
}

std::unique_ptr<views::View> CreateListItemView(size_t pos_in_set) {
  auto view = std::make_unique<views::View>();
  view->SetUseDefaultFillLayout(true);
  view->GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
  view->GetViewAccessibility().SetPosInSet(pos_in_set);
  // Setting the hierarchical level explicitly allows the SetSize to be
  // overridden later.
  view->GetViewAccessibility().SetHierarchicalLevel(1);
  return view;
}

}  // namespace

PickerImageItemGridView::PickerImageItemGridView(int grid_width)
    : grid_width_(grid_width),
      focus_search_(std::make_unique<FocusSearch>(
          this,
          base::BindRepeating(&PickerImageItemGridView::GetFocusableItems,
                              base::Unretained(this)))) {
  SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                  /*v_align=*/views::LayoutAlignment::kStretch,
                  /*horizontal_resize=*/1.0f,
                  /*size_type=*/views::TableLayout::ColumnSize::kFixed,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(
          /*horizontal_resize=*/views::TableLayout::kFixedSize,
          /*width=*/kImageGridPadding)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                 /*v_align=*/views::LayoutAlignment::kStretch,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kFixed,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize,
               /*height=*/0);

  SetProperty(views::kMarginsKey, kImageGridMargin);

  AddChildView(CreateImageGridColumn());
  AddChildView(CreateImageGridColumn());
}

PickerImageItemGridView::~PickerImageItemGridView() = default;

views::FocusTraversable* PickerImageItemGridView::GetPaneFocusTraversable() {
  return focus_search_.get();
}

views::View* PickerImageItemGridView::GetTopItem() {
  views::View* column = children().front();
  return column->children().empty()
             ? nullptr
             : column->children().front()->children().front().get();
}

views::View* PickerImageItemGridView::GetBottomItem() {
  views::View* tallest_column =
      base::ranges::max(children(),
                        /*comp=*/base::ranges::less(),
                        /*proj=*/[](const views::View* v) {
                          return v->GetPreferredSize().height();
                        });
  return tallest_column->children().empty()
             ? nullptr
             : tallest_column->children().back()->children().front().get();
}

views::View* PickerImageItemGridView::GetItemAbove(views::View* item) {
  views::View* column = GetColumnContaining(item);
  if (!column || item->parent() == column->children().front()) {
    return nullptr;
  }
  return std::prev(base::ranges::find(column->children(), item->parent()))
      ->get()
      ->children()
      .front()
      .get();
}

views::View* PickerImageItemGridView::GetItemBelow(views::View* item) {
  views::View* column = GetColumnContaining(item);
  if (!column || item->parent() == column->children().back()) {
    return nullptr;
  }
  return std::next(base::ranges::find(column->children(), item->parent()))
      ->get()
      ->children()
      .front()
      .get();
}

views::View* PickerImageItemGridView::GetItemLeftOf(views::View* item) {
  views::View* column = GetColumnContaining(item);
  if (!column || column == children().front()) {
    return nullptr;
  }
  // Prefer to return the item with the same index in the column to the left,
  // since this will probably be at a similar height to `item` (at least in
  // usual scenarios where the grid items all have similar dimensions).
  const size_t item_index = column->GetIndexOf(item->parent()).value();
  views::View* left_column =
      std::prev(base::ranges::find(children(), column))->get();
  return ItemInColumnWithIndexClosestTo(left_column, item_index);
}

views::View* PickerImageItemGridView::GetItemRightOf(views::View* item) {
  views::View* column = GetColumnContaining(item);
  if (!column || column == children().back()) {
    return nullptr;
  }
  // Prefer to return the item with the same index in the column to the right,
  // since this will probably be at a similar height to `item` (at least in
  // usual scenarios where the grid items all have similar dimensions).
  const size_t item_index = column->GetIndexOf(item->parent()).value();
  views::View* right_column =
      std::next(base::ranges::find(children(), column))->get();
  return ItemInColumnWithIndexClosestTo(right_column, item_index);
}

bool PickerImageItemGridView::ContainsItem(views::View* item) {
  return Contains(item);
}

PickerImageItemView* PickerImageItemGridView::AddImageItem(
    std::unique_ptr<PickerImageItemView> image_item) {
  views::View* shortest_column =
      base::ranges::min(children(),
                        /*comp=*/base::ranges::less(),
                        /*proj=*/[](const views::View* v) {
                          return v->GetPreferredSize().height();
                        });
  PickerImageItemView* new_item =
      shortest_column
          ->AddChildView(CreateListItemView(focusable_items_.size() + 1))
          ->AddChildView(std::move(image_item));
  focusable_items_.push_back(new_item);

  // Update the SetSize for all items.
  for (views::View* view : focusable_items_) {
    view->parent()->GetViewAccessibility().SetSetSize(focusable_items_.size());
  }

  return new_item;
}

PickerImageItemGridView::FocusSearch::FocusSearch(
    views::View* view,
    const GetFocusableViewsCallback& callback)
    : views::FocusSearch(/*root=*/view,
                         /*cycle=*/true,
                         /*accessibility_mode=*/true),
      view_(view),
      get_focusable_views_callback_(callback) {}

PickerImageItemGridView::FocusSearch::~FocusSearch() = default;

views::View* PickerImageItemGridView::FocusSearch::FindNextFocusableView(
    views::View* starting_view,
    SearchDirection search_direction,
    TraversalDirection traversal_direction,
    StartingViewPolicy check_starting_view,
    AnchoredDialogPolicy can_go_into_anchored_dialog,
    views::FocusTraversable** focus_traversable,
    views::View** focus_traversable_view) {
  // The callback polls the currently focusable views.
  const views::View::Views& focusable_views =
      get_focusable_views_callback_.Run();
  if (focusable_views.empty()) {
    return nullptr;
  }

  int delta =
      search_direction == FocusSearch::SearchDirection::kForwards ? 1 : -1;
  int focusable_views_size = static_cast<int>(focusable_views.size());
  for (int i = 0; i < focusable_views_size; ++i) {
    // If current view from the set is found to be focused, return the view
    // next (or previous) to it as next focusable view.
    if (focusable_views[i] == starting_view) {
      const int next_index = i + delta;
      if (next_index >= 0 && next_index < focusable_views_size) {
        return focusable_views[next_index];
      } else {
        return nullptr;
      }
    }
  }

  // Case when none of the views are already focused.
  return (search_direction == FocusSearch::SearchDirection::kForwards)
             ? focusable_views.front()
             : focusable_views.back();
}

views::FocusSearch* PickerImageItemGridView::FocusSearch::GetFocusSearch() {
  return this;
}

views::FocusTraversable*
PickerImageItemGridView::FocusSearch::GetFocusTraversableParent() {
  return nullptr;
}

views::View*
PickerImageItemGridView::FocusSearch::GetFocusTraversableParentView() {
  return nullptr;
}

views::View* PickerImageItemGridView::GetColumnContaining(views::View* item) {
  views::View* column =
      item->parent() == nullptr ? nullptr : item->parent()->parent();
  return column && column->parent() == this ? column : nullptr;
}

const views::View::Views& PickerImageItemGridView::GetFocusableItems() const {
  return focusable_items_;
}

BEGIN_METADATA(PickerImageItemGridView)
END_METADATA

}  // namespace ash
