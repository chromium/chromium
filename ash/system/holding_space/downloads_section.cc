// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/downloads_section.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/bubble/simple_grid_layout.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_section.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_ui.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Header ----------------------------------------------------------------------

class Header : public views::Button {
 public:
  Header() {
    SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_DOWNLOADS_TITLE));
    SetCallback(
        base::BindRepeating(&Header::OnPressed, base::Unretained(this)));
    SetID(kHoldingSpaceDownloadsSectionHeaderId);

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        kHoldingSpaceSectionHeaderSpacing));

    // Label.
    auto* label = AddChildView(
        holding_space_ui::CreateSectionHeaderLabel(
            IDS_ASH_HOLDING_SPACE_DOWNLOADS_TITLE)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
            .Build());
    layout->SetFlexForView(label, 1);

    // Chevron.
    chevron_ = AddChildView(std::make_unique<views::ImageView>());
    chevron_->SetFlipCanvasOnPaintForRTLUI(true);

    // Focus ring.
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
    AshColorProvider* const ash_color_provider = AshColorProvider::Get();

    // Chevron.
    chevron_->SetImage(gfx::CreateVectorIcon(
        kChevronRightSmallIcon, kHoldingSpaceSectionChevronIconSize,
        ash_color_provider->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary)));
  }

  void OnPressed() {
    holding_space_metrics::RecordDownloadsAction(
        holding_space_metrics::DownloadsAction::kClick);

    HoldingSpaceController::Get()->client()->OpenDownloads(base::DoNothing());
  }

  // Owned by view hierarchy.
  views::ImageView* chevron_ = nullptr;
};

}  // namespace

// DownloadsSection ------------------------------------------------------------

DownloadsSection::DownloadsSection(HoldingSpaceViewDelegate* delegate)
    : HoldingSpaceItemViewsSection(delegate,
                                   HoldingSpaceSectionId::kDownloads) {}

DownloadsSection::~DownloadsSection() = default;

const char* DownloadsSection::GetClassName() const {
  return "DownloadsSection";
}

std::unique_ptr<views::View> DownloadsSection::CreateHeader() {
  auto header = std::make_unique<Header>();
  header->SetPaintToLayer();
  header->layer()->SetFillsBoundsOpaquely(false);
  return header;
}

std::unique_ptr<views::View> DownloadsSection::CreateContainer() {
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<SimpleGridLayout>(
      kHoldingSpaceChipCountPerRow,
      /*column_spacing=*/kHoldingSpaceSectionContainerChildSpacing,
      /*row_spacing=*/kHoldingSpaceSectionContainerChildSpacing));
  return container;
}

std::unique_ptr<HoldingSpaceItemView> DownloadsSection::CreateView(
    const HoldingSpaceItem* item) {
  return std::make_unique<HoldingSpaceItemChipView>(delegate(), item);
}

}  // namespace ash
