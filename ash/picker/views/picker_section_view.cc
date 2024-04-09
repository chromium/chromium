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
#include "ash/picker/views/picker_traversable_item_container.h"
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
          .SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY)
          .Build());
}

PickerListItemView* PickerSectionView::AddListItem(
    std::unique_ptr<PickerListItemView> list_item) {
  if (list_item_container_ == nullptr) {
    list_item_container_ =
        AddChildView(std::make_unique<PickerListItemContainerView>());
  }
  PickerListItemView* list_item_ptr =
      list_item_container_->AddListItem(std::move(list_item));
  item_views_.push_back(list_item_ptr);
  return list_item_ptr;
}

PickerEmojiItemView* PickerSectionView::AddEmojiItem(
    std::unique_ptr<PickerEmojiItemView> emoji_item) {
  CreateSmallItemGridIfNeeded();
  PickerEmojiItemView* emoji_item_ptr =
      small_item_grid_->AddEmojiItem(std::move(emoji_item));
  item_views_.push_back(emoji_item_ptr);
  return emoji_item_ptr;
}

PickerSymbolItemView* PickerSectionView::AddSymbolItem(
    std::unique_ptr<PickerSymbolItemView> symbol_item) {
  CreateSmallItemGridIfNeeded();
  PickerSymbolItemView* symbol_item_ptr =
      small_item_grid_->AddSymbolItem(std::move(symbol_item));
  item_views_.push_back(symbol_item_ptr);
  return symbol_item_ptr;
}

PickerEmoticonItemView* PickerSectionView::AddEmoticonItem(
    std::unique_ptr<PickerEmoticonItemView> emoticon_item) {
  CreateSmallItemGridIfNeeded();
  PickerEmoticonItemView* emoticon_item_ptr =
      small_item_grid_->AddEmoticonItem(std::move(emoticon_item));
  item_views_.push_back(emoticon_item_ptr);
  return emoticon_item_ptr;
}

PickerImageItemView* PickerSectionView::AddImageItem(
    std::unique_ptr<PickerImageItemView> image_item) {
  if (image_item_grid_ == nullptr) {
    image_item_grid_ =
        AddChildView(std::make_unique<PickerImageItemGridView>(section_width_));
  }
  PickerImageItemView* image_item_ptr =
      image_item_grid_->AddImageItem(std::move(image_item));
  item_views_.push_back(image_item_ptr);
  return image_item_ptr;
}

PickerItemView* PickerSectionView::GetTopItem() {
  return GetItemContainer() != nullptr ? GetItemContainer()->GetTopItem()
                                       : nullptr;
}

PickerItemView* PickerSectionView::GetBottomItem() {
  return GetItemContainer() != nullptr ? GetItemContainer()->GetBottomItem()
                                       : nullptr;
}

PickerItemView* PickerSectionView::GetItemAbove(PickerItemView* item) {
  return GetItemContainer() != nullptr ? GetItemContainer()->GetItemAbove(item)
                                       : nullptr;
}

PickerItemView* PickerSectionView::GetItemBelow(PickerItemView* item) {
  return GetItemContainer() != nullptr ? GetItemContainer()->GetItemBelow(item)
                                       : nullptr;
}

PickerItemView* PickerSectionView::GetItemLeftOf(PickerItemView* item) {
  return GetItemContainer() != nullptr ? GetItemContainer()->GetItemLeftOf(item)
                                       : nullptr;
}

PickerItemView* PickerSectionView::GetItemRightOf(PickerItemView* item) {
  return GetItemContainer() != nullptr
             ? GetItemContainer()->GetItemRightOf(item)
             : nullptr;
}

void PickerSectionView::CreateSmallItemGridIfNeeded() {
  if (small_item_grid_ == nullptr) {
    small_item_grid_ =
        AddChildView(std::make_unique<PickerSmallItemGridView>(section_width_));
  }
}

PickerTraversableItemContainer* PickerSectionView::GetItemContainer() {
  if (list_item_container_ != nullptr) {
    return list_item_container_;
  } else if (image_item_grid_ != nullptr) {
    return image_item_grid_;
  } else {
    return small_item_grid_;
  }
}

BEGIN_METADATA(PickerSectionView)
END_METADATA

}  // namespace ash
