// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_image_item_grid_view.h"

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

namespace ash {
namespace {

// Padding between and around image grid items.
constexpr int kImageGridPadding = 8;

// Number of columns in an image grid.
constexpr int kNumImageGridColumns = 2;

int GetImageGridColumnWidth(int grid_width) {
  return (grid_width - (kNumImageGridColumns + 1) * kImageGridPadding) /
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

  SetProperty(views::kMarginsKey, gfx::Insets::VH(0, kImageGridPadding));

  AddChildView(CreateImageGridColumn());
  AddChildView(CreateImageGridColumn());
}

PickerImageItemGridView::~PickerImageItemGridView() = default;

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

BEGIN_METADATA(PickerImageItemGridView)
END_METADATA

}  // namespace ash
