// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_image_item_grid_view.h"

#include <iterator>
#include <memory>
#include <utility>

#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

// Padding between image grid items.
constexpr int kImageGridPadding = 8;

// Horizontal margin for the image grid.
constexpr int kImageGridMargin = 16;

// Number of columns in an image grid.
constexpr int kNumImageGridColumns = 2;

int GetImageGridColumnWidth(int grid_width) {
  return (grid_width - (kNumImageGridColumns - 1) * kImageGridPadding -
          kImageGridMargin * 2) /
         kNumImageGridColumns;
}

std::unique_ptr<views::View> CreateImageGridColumn() {
  auto column = views::Builder<views::FlexLayoutView>()
                    .SetOrientation(views::LayoutOrientation::kVertical)
                    .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                    .Build();
  column->SetDefault(views::kMarginsKey,
                     gfx::Insets::TLBR(0, 0, kImageGridPadding, 0));
  return column;
}

PickerItemView* ItemInColumnWithIndexClosestTo(views::View* column,
                                               const size_t index) {
  if (column->children().empty()) {
    return nullptr;
  } else if (index < column->children().size()) {
    return views::AsViewClass<PickerItemView>(column->children()[index].get());
  } else {
    return views::AsViewClass<PickerItemView>(column->children().back().get());
  }
}

}  // namespace

PickerImageItemGridView::PickerImageItemGridView(int grid_width)
    : grid_width_(grid_width) {
  SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                  /*v_align=*/views::LayoutAlignment::kStart,
                  /*horizontal_resize=*/1.0f,
                  /*size_type=*/views::TableLayout::ColumnSize::kFixed,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(
          /*horizontal_resize=*/views::TableLayout::kFixedSize,
          /*width=*/kImageGridPadding)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                 /*v_align=*/views::LayoutAlignment::kStart,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kFixed,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize,
               /*height=*/0);

  SetProperty(views::kMarginsKey, gfx::Insets::VH(0, kImageGridMargin));

  AddChildView(CreateImageGridColumn());
  AddChildView(CreateImageGridColumn());
}

PickerImageItemGridView::~PickerImageItemGridView() = default;

PickerItemView* PickerImageItemGridView::GetTopItem() {
  views::View* column = children().front();
  return column->children().empty() ? nullptr
                                    : views::AsViewClass<PickerItemView>(
                                          column->children().front().get());
}

PickerItemView* PickerImageItemGridView::GetBottomItem() {
  views::View* tallest_column =
      base::ranges::max(children(),
                        /*comp=*/base::ranges::less(),
                        /*proj=*/[](const views::View* v) {
                          return v->GetPreferredSize().height();
                        });
  return tallest_column->children().empty()
             ? nullptr
             : views::AsViewClass<PickerItemView>(
                   tallest_column->children().back().get());
}

PickerItemView* PickerImageItemGridView::GetItemAbove(PickerItemView* item) {
  views::View* column = GetColumnContaining(item);
  if (!column || item == column->children().front()) {
    return nullptr;
  }
  return views::AsViewClass<PickerItemView>(
      std::prev(base::ranges::find(column->children(), item))->get());
}

PickerItemView* PickerImageItemGridView::GetItemBelow(PickerItemView* item) {
  views::View* column = GetColumnContaining(item);
  if (!column || item == column->children().back()) {
    return nullptr;
  }
  return views::AsViewClass<PickerItemView>(
      std::next(base::ranges::find(column->children(), item))->get());
}

PickerItemView* PickerImageItemGridView::GetItemLeftOf(PickerItemView* item) {
  views::View* column = GetColumnContaining(item);
  if (!column || column == children().front()) {
    return nullptr;
  }
  // Prefer to return the item with the same index in the column to the left,
  // since this will probably be at a similar height to `item` (at least in
  // usual scenarios where the grid items all have similar dimensions).
  const size_t item_index = column->GetIndexOf(item).value();
  views::View* left_column =
      std::prev(base::ranges::find(children(), column))->get();
  return ItemInColumnWithIndexClosestTo(left_column, item_index);
}

PickerItemView* PickerImageItemGridView::GetItemRightOf(PickerItemView* item) {
  views::View* column = GetColumnContaining(item);
  if (!column || column == children().back()) {
    return nullptr;
  }
  // Prefer to return the item with the same index in the column to the right,
  // since this will probably be at a similar height to `item` (at least in
  // usual scenarios where the grid items all have similar dimensions).
  const size_t item_index = column->GetIndexOf(item).value();
  views::View* right_column =
      std::next(base::ranges::find(children(), column))->get();
  return ItemInColumnWithIndexClosestTo(right_column, item_index);
}

PickerImageItemView* PickerImageItemGridView::AddImageItem(
    std::unique_ptr<PickerImageItemView> image_item) {
  image_item->SetImageSizeFromWidth(GetImageGridColumnWidth(grid_width_));
  views::View* shortest_column =
      base::ranges::min(children(),
                        /*comp=*/base::ranges::less(),
                        /*proj=*/[](const views::View* v) {
                          return v->GetPreferredSize().height();
                        });
  return shortest_column->AddChildView(std::move(image_item));
}

views::View* PickerImageItemGridView::GetColumnContaining(
    PickerItemView* item) {
  views::View* column = item->parent();
  return column && column->parent() == this ? column : nullptr;
}

BEGIN_METADATA(PickerImageItemGridView)
END_METADATA

}  // namespace ash
