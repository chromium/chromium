// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_chip_button_base.h"

#include "ash/style/style_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/metadata/view_factory_internal.h"

namespace ash {

namespace {

constexpr int kRoundedCornerRadius = 20;
constexpr ui::ColorId kBackgroundColorId = cros_tokens::kCrosSysSystemOnBase;
constexpr ui::ColorId kCoralSelectionShownBackgroundColorId =
    cros_tokens::kCrosSysSystemOnBaseOpaque;

}  // namespace

BirchChipButtonBase::BirchChipButtonBase() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  UpdateRoundedCorners(/*selection_widget_visible=*/false);
}

BirchChipButtonBase::~BirchChipButtonBase() = default;

void BirchChipButtonBase::UpdateRoundedCorners(bool selection_widget_visible) {
  const gfx::RoundedCornersF rounded_corners =
      selection_widget_visible
          ? gfx::RoundedCornersF(0, 0, kRoundedCornerRadius,
                                 kRoundedCornerRadius)
          : gfx::RoundedCornersF(kRoundedCornerRadius);
  SetBorder(std::make_unique<views::HighlightBorder>(
      rounded_corners, views::HighlightBorder::Type::kHighlightBorderNoShadow));
  SetBackground(views::CreateThemedRoundedRectBackground(
      selection_widget_visible ? kCoralSelectionShownBackgroundColorId
                               : kBackgroundColorId,
      rounded_corners));

  // Install and stylize the focus ring.
  StyleUtil::InstallRoundedCornerHighlightPathGenerator(this, rounded_corners);
  StyleUtil::SetUpFocusRingForView(this);
}

BEGIN_METADATA(BirchChipButtonBase)
END_METADATA

}  // namespace ash
