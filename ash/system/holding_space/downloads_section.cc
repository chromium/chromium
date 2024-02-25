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
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_ui.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
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
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

// Helpers ---------------------------------------------------------------------

std::unique_ptr<views::BoxLayout> WithCrossAxisAlignment(
    std::unique_ptr<views::BoxLayout> layout,
    views::BoxLayout::CrossAxisAlignment cross_axis_alignment) {
  layout->set_cross_axis_alignment(cross_axis_alignment);
  return layout;
}

// Header ----------------------------------------------------------------------

class Header : public views::Button {
  METADATA_HEADER(Header, views::Button)

 public:
  Header() {
    // Layout/Properties.
    views::Builder<views::Button>(this)
        .SetID(kHoldingSpaceDownloadsSectionHeaderId)
        .SetAccessibleName(
            l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_DOWNLOADS_TITLE))
        .SetCallback(
            base::BindRepeating(&Header::OnPressed, base::Unretained(this)))
        .SetLayoutManager(WithCrossAxisAlignment(
            std::make_unique<views::BoxLayout>(
                views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
                kHoldingSpaceSectionHeaderSpacing),
            views::BoxLayout::CrossAxisAlignment::kCenter))
        .AddChildren(
            holding_space_ui::CreateSectionHeaderLabel(
                IDS_ASH_HOLDING_SPACE_DOWNLOADS_TITLE)
                .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                .SetProperty(views::kBoxLayoutFlexKey,
                             views::BoxLayoutFlexSpecification()),
            views::Builder<views::ImageView>()
                .CopyAddressTo(&chevron_)
                .SetFlipCanvasOnPaintForRTLUI(true)
                .SetImage(ui::ImageModel::FromVectorIcon(
                    kChevronRightSmallIcon, kColorAshIconColorPrimary,
                    kHoldingSpaceSectionChevronIconSize)))
        .BuildChildren();

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
  void OnPressed() {
    holding_space_metrics::RecordDownloadsAction(
        holding_space_metrics::DownloadsAction::kClick);

    HoldingSpaceController::Get()->client()->OpenDownloads(base::DoNothing());
  }

  // Owned by view hierarchy.
  raw_ptr<views::ImageView> chevron_ = nullptr;
};

BEGIN_METADATA(Header)
END_METADATA

}  // namespace

// DownloadsSection ------------------------------------------------------------

DownloadsSection::DownloadsSection(HoldingSpaceViewDelegate* delegate)
    : HoldingSpaceItemViewsSection(delegate,
                                   HoldingSpaceSectionId::kDownloads) {}

DownloadsSection::~DownloadsSection() = default;

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

BEGIN_METADATA(DownloadsSection)
END_METADATA

}  // namespace ash
