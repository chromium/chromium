// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/screen_captures_section.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_section.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_item_screen_capture_view.h"
#include "ash/system/holding_space/holding_space_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

ScreenCapturesSection::ScreenCapturesSection(HoldingSpaceViewDelegate* delegate)
    : HoldingSpaceItemViewsSection(delegate,
                                   HoldingSpaceSectionId::kScreenCaptures) {}

ScreenCapturesSection::~ScreenCapturesSection() = default;

std::unique_ptr<views::View> ScreenCapturesSection::CreateHeader() {
  auto header =
      holding_space_ui::CreateSectionHeaderLabel(
          IDS_ASH_HOLDING_SPACE_SCREEN_CAPTURES_TITLE)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetPaintToLayer()
          .Build();
  header->layer()->SetFillsBoundsOpaquely(false);
  return header;
}

std::unique_ptr<views::View> ScreenCapturesSection::CreateContainer() {
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, 0, 0,
                                    kHoldingSpaceSectionContainerChildSpacing));
  return container;
}

std::unique_ptr<HoldingSpaceItemView> ScreenCapturesSection::CreateView(
    const HoldingSpaceItem* item) {
  return std::make_unique<HoldingSpaceItemScreenCaptureView>(delegate(), item);
}

BEGIN_METADATA(ScreenCapturesSection)
END_METADATA

}  // namespace ash
