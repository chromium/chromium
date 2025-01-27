// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_image_item_grid_view.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <utility>

#include "ash/quick_insert/views/quick_insert_image_item_view.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_traversable_item_container.h"
#include "base/notimplemented.h"
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

std::unique_ptr<views::View> CreateListItemView() {
  auto view = std::make_unique<views::View>();
  view->SetUseDefaultFillLayout(true);
  view->GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
  // Setting the hierarchical level explicitly allows the SetSize to be
  // overridden later.
  view->GetViewAccessibility().SetHierarchicalLevel(1);
  return view;
}

}  // namespace

QuickInsertImageItemGridView::QuickInsertImageItemGridView(int grid_width,
                                                           bool has_top_margin)
    : column_width_(
          (grid_width - kImageGridPadding - kImageGridMargin.width()) / 2) {
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

  gfx::Insets margins = kImageGridMargin;
  if (!has_top_margin) {
    margins.set_top(0);
  }
  SetProperty(views::kMarginsKey, margins);

  AddChildView(CreateImageGridColumn());
  AddChildView(CreateImageGridColumn());
}

QuickInsertImageItemGridView::~QuickInsertImageItemGridView() = default;

views::View* QuickInsertImageItemGridView::GetTopItem() {
  views::View* column = children().front();
  return column->children().empty()
             ? nullptr
             : column->children().front()->children().front().get();
}

views::View* QuickInsertImageItemGridView::GetBottomItem() {
  views::View* tallest_column =
      std::ranges::max(children(),
                       /*comp=*/std::ranges::less(),
                       /*proj=*/[](const views::View* v) {
                         return v->GetPreferredSize().height();
                       });
  return tallest_column->children().empty()
             ? nullptr
             : tallest_column->children().back()->children().front().get();
}

views::View* QuickInsertImageItemGridView::GetItemAbove(views::View* item) {
  views::View* column = GetColumnContaining(item);
  if (!column || item->parent() == column->children().front()) {
    return nullptr;
  }
  return std::prev(std::ranges::find(column->children(), item->parent()))
      ->get()
      ->children()
      .front()
      .get();
}

views::View* QuickInsertImageItemGridView::GetItemBelow(views::View* item) {
  views::View* column = GetColumnContaining(item);
  if (!column || item->parent() == column->children().back()) {
    return nullptr;
  }
  return std::next(std::ranges::find(column->children(), item->parent()))
      ->get()
      ->children()
      .front()
      .get();
}

views::View* QuickInsertImageItemGridView::GetItemLeftOf(views::View* item) {
  views::View* column = GetColumnContaining(item);
  if (!column || column == children().front()) {
    return nullptr;
  }
  // Prefer to return the item with the same index in the column to the left,
  // since this will probably be at a similar height to `item` (at least in
  // usual scenarios where the grid items all have similar dimensions).
  const size_t item_index = column->GetIndexOf(item->parent()).value();
  views::View* left_column =
      std::prev(std::ranges::find(children(), column))->get();
  return ItemInColumnWithIndexClosestTo(left_column, item_index);
}

views::View* QuickInsertImageItemGridView::GetItemRightOf(views::View* item) {
  views::View* column = GetColumnContaining(item);
  if (!column || column == children().back()) {
    return nullptr;
  }
  // Prefer to return the item with the same index in the column to the right,
  // since this will probably be at a similar height to `item` (at least in
  // usual scenarios where the grid items all have similar dimensions).
  const size_t item_index = column->GetIndexOf(item->parent()).value();
  views::View* right_column =
      std::next(std::ranges::find(children(), column))->get();
  return ItemInColumnWithIndexClosestTo(right_column, item_index);
}

bool QuickInsertImageItemGridView::ContainsItem(views::View* item) {
  return Contains(item);
}

QuickInsertImageItemView* QuickInsertImageItemGridView::AddImageItem(
    std::unique_ptr<QuickInsertImageItemView> image_item) {
  views::View* shortest_column =
      std::ranges::min(children(),
                       /*comp=*/std::ranges::less(),
                       /*proj=*/[](const views::View* v) {
                         return v->GetPreferredSize().height();
                       });
  QuickInsertImageItemView* new_item =
      shortest_column->AddChildView(CreateListItemView())
          ->AddChildView(std::move(image_item));
  new_item->FitToWidth(column_width_);
  // Only the first item in the grid should be focusable.
  if (num_items_ > 0) {
    new_item->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  }
  ++num_items_;

  // Update the PosInSet and SetSize for all items, column by column.
  size_t pos_in_set = 1;
  for (views::View* column : children()) {
    for (views::View* item : column->children()) {
      item->GetViewAccessibility().SetPosInSet(pos_in_set);
      item->GetViewAccessibility().SetSetSize(num_items_);
      ++pos_in_set;
    }
  }

  return new_item;
}

views::View* QuickInsertImageItemGridView::GetColumnContaining(
    views::View* item) {
  views::View* column =
      item->parent() == nullptr ? nullptr : item->parent()->parent();
  return column && column->parent() == this ? column : nullptr;
}

BEGIN_METADATA(QuickInsertImageItemGridView)
END_METADATA

}  // namespace ash
