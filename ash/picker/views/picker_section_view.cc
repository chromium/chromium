// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_section_view.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/style/typography.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr auto kSectionTitlePadding = gfx::Insets::VH(8, 16);

// Horizontal padding between small grid items.
constexpr auto kSmallGridItemMargins = gfx::Insets::VH(0, 12);

// Padding around each row of small items.
constexpr auto kSmallGridItemRowMargins = gfx::Insets::TLBR(0, 8, 8, 8);

// Padding between and around large grid items.
constexpr int kLargeGridItemsPadding = 8;

std::unique_ptr<views::View> CreateSmallGridItemsRow() {
  auto row = views::Builder<views::FlexLayoutView>()
                 .SetOrientation(views::LayoutOrientation::kHorizontal)
                 .SetMainAxisAlignment(views::LayoutAlignment::kStart)
                 .SetCollapseMargins(true)
                 .SetIgnoreDefaultMainAxisMargins(true)
                 .SetProperty(views::kMarginsKey, kSmallGridItemRowMargins)
                 .Build();
  row->SetDefault(views::kMarginsKey, kSmallGridItemMargins);
  return row;
}

std::unique_ptr<views::View> CreateSmallGridItemsContainer() {
  return views::Builder<views::FlexLayoutView>()
      .SetOrientation(views::LayoutOrientation::kVertical)
      .Build();
}

std::unique_ptr<views::View> CreateLargeGridItemsColumn() {
  auto column = views::Builder<views::FlexLayoutView>()
                    .SetOrientation(views::LayoutOrientation::kVertical)
                    .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                    .Build();
  column->SetDefault(views::kMarginsKey,
                     gfx::Insets::TLBR(0, 0, kLargeGridItemsPadding, 0));
  return column;
}

std::unique_ptr<views::View> CreateLargeGridItemsContainer() {
  auto container =
      views::Builder<views::TableLayoutView>()
          .AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                     /*v_align=*/views::LayoutAlignment::kStart,
                     /*horizontal_resize=*/1.0f,
                     /*size_type=*/views::TableLayout::ColumnSize::kFixed,
                     /*fixed_width=*/0, /*min_width=*/0)
          .AddPaddingColumn(
              /*horizontal_resize=*/views::TableLayout::kFixedSize,
              /*width=*/kLargeGridItemsPadding)
          .AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                     /*v_align=*/views::LayoutAlignment::kStart,
                     /*horizontal_resize=*/1.0f,
                     /*size_type=*/views::TableLayout::ColumnSize::kFixed,
                     /*fixed_width=*/0, /*min_width=*/0)
          .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize,
                   /*height=*/0)
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::VH(0, kLargeGridItemsPadding))
          .Build();
  container->AddChildView(CreateLargeGridItemsColumn());
  container->AddChildView(CreateLargeGridItemsColumn());
  return container;
}

std::unique_ptr<views::View> CreateListItemsContainer() {
  return views::Builder<views::FlexLayoutView>()
      .SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .Build();
}

}  // namespace

PickerSectionView::PickerSectionView(const std::u16string& title_text) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  title_ = AddChildView(
      bubble_utils::CreateLabel(TypographyToken::kCrosAnnotation2, title_text,
                                cros_tokens::kCrosSysOnSurfaceVariant));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetBorder(views::CreateEmptyBorder(kSectionTitlePadding));
}

PickerSectionView::~PickerSectionView() = default;

void PickerSectionView::SetMaximumWidth(int maximum_width) {
  maximum_width_ = maximum_width;
}

void PickerSectionView::AddItem(std::unique_ptr<PickerItemView> item_view) {
  switch (item_view->item_type()) {
    case PickerItemView::ItemType::kSmallGridItem:
      AddSmallGridItem(std::move(item_view));
      break;
    case PickerItemView::ItemType::kLargeGridItem:
      AddLargeGridItem(std::move(item_view));
      break;
    case PickerItemView::ItemType::kListItem:
      AddListItem(std::move(item_view));
      break;
  }
}

void PickerSectionView::AddSmallGridItem(
    std::unique_ptr<PickerItemView> item_view) {
  if (small_grid_items_container_ == nullptr) {
    small_grid_items_container_ = AddChildView(CreateSmallGridItemsContainer());
    small_grid_items_container_->AddChildView(CreateSmallGridItemsRow());
  }

  // Try to add the item to the last row. If it doesn't fit, create a new row
  // and add the item there.
  views::View* row = small_grid_items_container_->children().back();
  if (!row->children().empty() && maximum_width_.has_value() &&
      row->GetPreferredSize().width() + kSmallGridItemMargins.left() +
              item_view->GetPreferredSize().width() >
          maximum_width_.value()) {
    row = small_grid_items_container_->AddChildView(CreateSmallGridItemsRow());
  }
  item_views_.push_back(row->AddChildView(std::move(item_view)));
}

void PickerSectionView::AddLargeGridItem(
    std::unique_ptr<PickerItemView> item_view) {
  if (large_grid_items_container_ == nullptr) {
    large_grid_items_container_ = AddChildView(CreateLargeGridItemsContainer());
  }

  views::View* shortest_column =
      base::ranges::min(large_grid_items_container_->children(),
                        /*comp=*/base::ranges::less(),
                        /*proj=*/[](const views::View* v) {
                          return v->GetPreferredSize().height();
                        });
  item_views_.push_back(shortest_column->AddChildView(std::move(item_view)));
}

void PickerSectionView::AddListItem(std::unique_ptr<PickerItemView> item_view) {
  if (list_items_container_ == nullptr) {
    list_items_container_ = AddChildView(CreateListItemsContainer());
  }
  item_views_.push_back(
      list_items_container_->AddChildView(std::move(item_view)));
}

BEGIN_METADATA(PickerSectionView)
END_METADATA

}  // namespace ash
