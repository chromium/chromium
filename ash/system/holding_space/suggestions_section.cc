// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/suggestions_section.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/bubble/simple_grid_layout.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/metadata/view_factory_internal.h"

namespace ash {

namespace {

// Header ----------------------------------------------------------------------

class Header : public views::Button {
 public:
  Header() {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        kHoldingSpaceSectionHeaderSpacing));

    views::Label* label = nullptr;
    views::Builder<views::Button>(this)
        .SetAccessibleName(
            l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_SUGGESTIONS_TITLE))
        .AddChildren(
            views::Builder<views::Label>(
                bubble_utils::CreateLabel(
                    bubble_utils::LabelStyle::kSubheader,
                    l10n_util::GetStringUTF16(
                        IDS_ASH_HOLDING_SPACE_SUGGESTIONS_TITLE)))
                .CopyAddressTo(&label)
                .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT),
            views::Builder<views::ImageView>().CopyAddressTo(&chevron_))
        .BuildChildren();

    layout->SetFlexForView(label, 1);

    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }

 private:
  // views::Button:
  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    AshColorProvider* const ash_color_provider = AshColorProvider::Get();

    // Rotate `kChevronRightIcon` to match the chevron in the downloads section.
    // `kChevronUpIcon` would appear larger due to different internal padding.
    chevron_->SetImage(gfx::ImageSkiaOperations::CreateRotatedImage(
        gfx::CreateVectorIcon(
            kChevronRightIcon, kHoldingSpaceSectionChevronIconSize,
            ash_color_provider->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kIconColorSecondary)),
        SkBitmapOperations::ROTATION_270_CW));
  }

  // Owned by view hierarchy.
  views::ImageView* chevron_ = nullptr;
};

}  // namespace

// SuggestionsSection ----------------------------------------------------------

SuggestionsSection::SuggestionsSection(HoldingSpaceViewDelegate* delegate)
    : HoldingSpaceItemViewsSection(delegate,
                                   /*supported_types=*/
                                   {HoldingSpaceItem::Type::kDriveSuggestion,
                                    HoldingSpaceItem::Type::kLocalSuggestion},
                                   /*max_count=*/kMaxSuggestions) {
  SetID(kHoldingSpaceSuggestionsSectionId);
}

SuggestionsSection::~SuggestionsSection() = default;

const char* SuggestionsSection::GetClassName() const {
  return "SuggestionsSection";
}

std::unique_ptr<views::View> SuggestionsSection::CreateHeader() {
  return std::make_unique<Header>();
}

std::unique_ptr<views::View> SuggestionsSection::CreateContainer() {
  return views::Builder<views::View>()
      .SetLayoutManager(std::make_unique<SimpleGridLayout>(
          kHoldingSpaceChipCountPerRow,
          /*column_spacing=*/kHoldingSpaceSectionContainerChildSpacing,
          /*row_spacing=*/kHoldingSpaceSectionContainerChildSpacing))
      .Build();
}

std::unique_ptr<HoldingSpaceItemView> SuggestionsSection::CreateView(
    const HoldingSpaceItem* item) {
  return std::make_unique<HoldingSpaceItemChipView>(delegate(), item);
}

}  // namespace ash
