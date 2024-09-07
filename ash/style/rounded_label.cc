// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/rounded_label.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_id.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/highlight_border.h"

namespace ash {

RoundedLabel::RoundedLabel(int horizontal_padding,
                           int vertical_padding,
                           int rounding_dp,
                           int preferred_height,
                           const std::u16string& text)
    : views::Label(text, views::style::CONTEXT_LABEL),
      rounding_dp_(rounding_dp),
      preferred_height_(preferred_height) {
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(vertical_padding, horizontal_padding)));
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  SetEnabledColorId(kColorAshTextColorPrimary);
  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(rounding_dp));
  layer()->SetIsFastRoundedCorner(true);
}

RoundedLabel::~RoundedLabel() = default;

gfx::Size RoundedLabel::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(views::Label::CalculatePreferredSize(available_size).width(),
                   preferred_height_);
}

void RoundedLabel::OnPaintBorder(gfx::Canvas* canvas) {
  views::HighlightBorder::PaintBorderToCanvas(
      canvas, *this, GetLocalBounds(), gfx::RoundedCornersF(rounding_dp_),
      chromeos::features::IsJellyrollEnabled()
          ? views::HighlightBorder::Type::kHighlightBorderNoShadow
          : views::HighlightBorder::Type::kHighlightBorder2);
}

BEGIN_METADATA(RoundedLabel)
END_METADATA

}  // namespace ash
