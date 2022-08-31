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
#include "ash/system/holding_space/holding_space_util.h"
#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
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
        .SetID(kHoldingSpaceSuggestionsSectionHeaderId)
        .SetAccessibleName(
            l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_SUGGESTIONS_TITLE))
        .SetCallback(
            base::BindRepeating(&Header::OnPressed, base::Unretained(this)))
        .AddChildren(
            views::Builder<views::Label>(
                bubble_utils::CreateLabel(
                    bubble_utils::LabelStyle::kSubheader,
                    l10n_util::GetStringUTF16(
                        IDS_ASH_HOLDING_SPACE_SUGGESTIONS_TITLE)))
                .CopyAddressTo(&label)
                .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT),
            views::Builder<views::ImageView>().CopyAddressTo(&chevron_).SetID(
                kHoldingSpaceSuggestionsChevronIconId))
        .BuildChildren();

    layout->SetFlexForView(label, 1);

    // Though the entirety of the header is focusable and behaves as a single
    // button, the focus ring is drawn as a circle around just the `chevron_`.
    views::FocusRing::Get(this)->SetPathGenerator(
        holding_space_util::CreateHighlightPathGenerator(base::BindRepeating(
            [](const views::View* chevron) {
              const float radius = chevron->width() / 2.f;
              gfx::RRectF path(gfx::RectF(chevron->bounds()), radius);
              if (base::i18n::IsRTL()) {
                // Manually adjust for flipped canvas in RTL.
                path.Offset(-chevron->parent()->width(), 0.f);
                path.Scale(-1.f, 1.f);
              }
              return path;
            },
            base::Unretained(chevron_))));
    views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
  }

 private:
  // views::Button:
  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    UpdateChevron();
  }

  void OnPressed() {
    // TODO(crbug/1358547): Record toggling the suggestions section's expanded
    // state in a histogram.
    expanded_ = !expanded_;
    UpdateChevron();
  }

  // Sets the header's `chevron_` icon to the correct color (based on theme) and
  // orientation (based on whether the `section_` is `expanded_`).
  void UpdateChevron() {
    chevron_->SetImage(gfx::ImageSkiaOperations::CreateRotatedImage(
        gfx::CreateVectorIcon(
            kChevronRightIcon, kHoldingSpaceSectionChevronIconSize,
            AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kIconColorSecondary)),
        // Rotate `kChevronRightIcon` to the correct orientation rather than use
        // `kChevronUpIcon` or `kChevronDownIcon`, which both have different
        // internal padding and therefore appear as a different size than the
        // downloads section's chevron.
        expanded_ ? SkBitmapOperations::ROTATION_270_CW
                  : SkBitmapOperations::ROTATION_90_CW));
  }

  // Owned by view hierarchy.
  views::ImageView* chevron_ = nullptr;

  // Determines the orientation of `chevron_`.
  // TODO(crbug/1358331): Update this comment when expanded state also affects
  // whether suggestions are shown.
  bool expanded_ = true;
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
