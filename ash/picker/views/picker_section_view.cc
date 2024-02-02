// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_section_view.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_emoticon_item_view.h"
#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/style/typography.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
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

// Preferred size of small grid items.
constexpr gfx::Size kSmallGridItemPreferredSize(32, 32);

// Padding between and around image grid items.
constexpr int kImageGridPadding = 8;

// TODO: b/323279115 - Compute this in terms of available width.
constexpr int kImageGridColumnWidth = 148;

std::unique_ptr<views::View> CreateSmallItemsGridRow() {
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

std::unique_ptr<views::View> CreateSmallItemsGrid() {
  return views::Builder<views::FlexLayoutView>()
      .SetOrientation(views::LayoutOrientation::kVertical)
      .Build();
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

std::unique_ptr<views::View> CreateImageGrid() {
  auto container =
      views::Builder<views::TableLayoutView>()
          .AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
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
                   /*height=*/0)
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::VH(0, kImageGridPadding))
          .Build();
  container->AddChildView(CreateImageGridColumn());
  container->AddChildView(CreateImageGridColumn());
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

void PickerSectionView::AddListItem(std::unique_ptr<views::View> list_item) {
  if (list_items_container_ == nullptr) {
    list_items_container_ = AddChildView(CreateListItemsContainer());
  }
  item_views_.push_back(
      list_items_container_->AddChildView(std::move(list_item)));
}

void PickerSectionView::AddEmojiItem(
    std::unique_ptr<PickerEmojiItemView> emoji_item) {
  emoji_item->SetPreferredSize(kSmallGridItemPreferredSize);
  AddSmallGridItem(std::move(emoji_item));
}

void PickerSectionView::AddSymbolItem(
    std::unique_ptr<PickerSymbolItemView> symbol_item) {
  symbol_item->SetPreferredSize(kSmallGridItemPreferredSize);
  AddSmallGridItem(std::move(symbol_item));
}

void PickerSectionView::AddEmoticonItem(
    std::unique_ptr<PickerEmoticonItemView> emoticon_item) {
  emoticon_item->SetPreferredSize(
      gfx::Size(std::max(emoticon_item->GetPreferredSize().width(),
                         kSmallGridItemPreferredSize.width()),
                kSmallGridItemPreferredSize.height()));
  AddSmallGridItem(std::move(emoticon_item));
}

void PickerSectionView::AddImageItem(
    std::unique_ptr<PickerImageItemView> image_item) {
  if (image_grid_ == nullptr) {
    image_grid_ = AddChildView(CreateImageGrid());
  }

  image_item->SetImageSizeFromWidth(kImageGridColumnWidth);
  views::View* shortest_column =
      base::ranges::min(image_grid_->children(),
                        /*comp=*/base::ranges::less(),
                        /*proj=*/[](const views::View* v) {
                          return v->GetPreferredSize().height();
                        });
  item_views_.push_back(shortest_column->AddChildView(std::move(image_item)));
}

void PickerSectionView::AddSmallGridItem(
    std::unique_ptr<views::View> grid_item) {
  if (small_items_grid_ == nullptr) {
    small_items_grid_ = AddChildView(CreateSmallItemsGrid());
    small_items_grid_->AddChildView(CreateSmallItemsGridRow());
  }

  // Try to add the item to the last row. If it doesn't fit, create a new row
  // and add the item there.
  views::View* row = small_items_grid_->children().back();
  if (!row->children().empty() && maximum_width_.has_value() &&
      row->GetPreferredSize().width() + kSmallGridItemMargins.left() +
              grid_item->GetPreferredSize().width() >
          maximum_width_.value()) {
    row = small_items_grid_->AddChildView(CreateSmallItemsGridRow());
  }
  item_views_.push_back(row->AddChildView(std::move(grid_item)));
}

BEGIN_METADATA(PickerSectionView)
END_METADATA

}  // namespace ash
