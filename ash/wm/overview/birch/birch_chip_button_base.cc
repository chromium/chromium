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

}  // namespace

BirchChipButtonBase::BirchChipButtonBase() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  UpdateRoundedCorners();
}

BirchChipButtonBase::~BirchChipButtonBase() = default;

void BirchChipButtonBase::SetTopHalfRounded(bool rounded) {
  if (top_half_rounded_ == rounded) {
    return;
  }

  top_half_rounded_ = rounded;
  UpdateRoundedCorners();
}

void BirchChipButtonBase::UpdateRoundedCorners() {
  const gfx::RoundedCornersF rounded_corners =
      top_half_rounded_ ? gfx::RoundedCornersF(kRoundedCornerRadius)
                        : gfx::RoundedCornersF(0, 0, kRoundedCornerRadius,
                                               kRoundedCornerRadius);
  SetBorder(std::make_unique<views::HighlightBorder>(
      rounded_corners, views::HighlightBorder::Type::kHighlightBorderNoShadow));
  SetBackground(views::CreateThemedRoundedRectBackground(kBackgroundColorId,
                                                         rounded_corners));

  // Install and stylize the focus ring.
  StyleUtil::InstallRoundedCornerHighlightPathGenerator(this, rounded_corners);
  StyleUtil::SetUpFocusRingForView(this);
}

BEGIN_METADATA(BirchChipButtonBase)
END_METADATA

}  // namespace ash
