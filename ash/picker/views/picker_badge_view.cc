// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_badge_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/typography.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr gfx::Size kIconSize(10, 10);
// Vertical padding is dynamic. The badge has a fixed height and the children
// are centered vertically.
constexpr auto kBadgePadding = gfx::Insets::VH(0, 5);
constexpr int kBadgeHeight = 20;
constexpr int kLabelRightPadding = 3;
constexpr int kBadgeCornerRadius = 4;

}  // namespace

PickerBadgeView::PickerBadgeView() {
  // TODO: b/342478227 - Ensure this works with tall text.
  views::Builder<views::BoxLayoutView>(this)
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetBackground(views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysHoverOnSubtle, kBadgeCornerRadius))
      .SetBorder(views::CreateEmptyBorder(kBadgePadding))
      .AddChildren(
          views::Builder<views::Label>()
              .CopyAddressTo(&label_)
              .SetProperty(views::kMarginsKey,
                           gfx::Insets::TLBR(0, 0, 0, kLabelRightPadding))
              .SetFontList(
                  ash::TypographyProvider::Get()->ResolveTypographyToken(
                      ash::TypographyToken::kCrosLabel1)),
          views::Builder<views::ImageView>()
              .SetImage(ui::ImageModel::FromVectorIcon(
                  kPickerReturnIcon, cros_tokens::kCrosSysOnSurface))
              .SetImageSize(kIconSize))
      .BuildChildren();
}

PickerBadgeView::~PickerBadgeView() = default;

const std::u16string& PickerBadgeView::GetText() const {
  return label_->GetText();
}

void PickerBadgeView::SetText(const std::u16string& text) {
  label_->SetText(std::move(text));
}

gfx::Size PickerBadgeView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_width =
      BoxLayoutView::CalculatePreferredSize(available_size).width();
  return gfx::Size(preferred_width, kBadgeHeight);
}

BEGIN_METADATA(PickerBadgeView)
END_METADATA

}  // namespace ash
