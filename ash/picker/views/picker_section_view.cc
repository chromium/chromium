// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_section_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_emoticon_item_view.h"
#include "ash/picker/views/picker_image_item_grid_view.h"
#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_container_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_small_item_grid_view.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/style/typography.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr auto kSectionTitleMargins = gfx::Insets::VH(8, 16);
constexpr auto kSectionTitleTrailingLinkMargins =
    gfx::Insets::TLBR(4, 8, 4, 16);

}  // namespace

PickerSectionView::PickerSectionView(int section_width)
    : section_width_(section_width) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  title_container_ =
      AddChildView(views::Builder<views::FlexLayoutView>()
                       .SetOrientation(views::LayoutOrientation::kHorizontal)
                       .Build());
}

PickerSectionView::~PickerSectionView() = default;

void PickerSectionView::AddTitleLabel(const std::u16string& title_text) {
  title_label_ = title_container_->AddChildView(
      views::Builder<views::Label>(
          bubble_utils::CreateLabel(TypographyToken::kCrosAnnotation2,
                                    title_text,
                                    cros_tokens::kCrosSysOnSurfaceVariant))
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::MinimumFlexSizeRule::kScaleToMinimum,
                           views::MaximumFlexSizeRule::kUnbounded)
                           .WithWeight(1))
          .SetProperty(views::kMarginsKey, kSectionTitleMargins)
          .Build());
}

void PickerSectionView::AddTitleTrailingLink(
    const std::u16string& link_text,
    views::Link::ClickedCallback link_callback) {
  title_trailing_link_ = title_container_->AddChildView(
      views::Builder<views::Link>()
          .SetText(link_text)
          .SetCallback(link_callback)
          .SetFontList(ash::TypographyProvider::Get()->ResolveTypographyToken(
              ash::TypographyToken::kCrosAnnotation2))
          .SetEnabledColorId(cros_tokens::kCrosSysPrimary)
          .SetForceUnderline(false)
          .SetProperty(views::kMarginsKey, kSectionTitleTrailingLinkMargins)
          .Build());
}

void PickerSectionView::AddListItem(
    std::unique_ptr<PickerListItemView> list_item) {
  if (list_item_container_ == nullptr) {
    list_item_container_ =
        AddChildView(std::make_unique<PickerListItemContainerView>());
  }
  item_views_.push_back(
      list_item_container_->AddListItem(std::move(list_item)));
}

void PickerSectionView::AddEmojiItem(
    std::unique_ptr<PickerEmojiItemView> emoji_item) {
  CreateSmallItemGridIfNeeded();
  item_views_.push_back(small_item_grid_->AddEmojiItem(std::move(emoji_item)));
}

void PickerSectionView::AddSymbolItem(
    std::unique_ptr<PickerSymbolItemView> symbol_item) {
  CreateSmallItemGridIfNeeded();
  item_views_.push_back(
      small_item_grid_->AddSymbolItem(std::move(symbol_item)));
}

void PickerSectionView::AddEmoticonItem(
    std::unique_ptr<PickerEmoticonItemView> emoticon_item) {
  CreateSmallItemGridIfNeeded();
  item_views_.push_back(
      small_item_grid_->AddEmoticonItem(std::move(emoticon_item)));
}

void PickerSectionView::AddImageItem(
    std::unique_ptr<PickerImageItemView> image_item) {
  if (image_item_grid_ == nullptr) {
    image_item_grid_ =
        AddChildView(std::make_unique<PickerImageItemGridView>(section_width_));
  }
  item_views_.push_back(image_item_grid_->AddImageItem(std::move(image_item)));
}

void PickerSectionView::CreateSmallItemGridIfNeeded() {
  if (small_item_grid_ == nullptr) {
    small_item_grid_ =
        AddChildView(std::make_unique<PickerSmallItemGridView>(section_width_));
  }
}

BEGIN_METADATA(PickerSectionView)
END_METADATA

}  // namespace ash
