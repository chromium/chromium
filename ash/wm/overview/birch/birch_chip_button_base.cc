// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_chip_button_base.h"

#include "ash/style/style_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/metadata/view_factory_internal.h"

namespace ash {

namespace {

constexpr int kRoundedCornerRadius = 20;
constexpr ui::ColorId kBackgroundColorId = cros_tokens::kCrosSysSystemOnBase;

}  // namespace

BirchChipButtonBase::BirchChipButtonBase() {
  SetBorder(std::make_unique<views::HighlightBorder>(
      kRoundedCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderNoShadow));
  SetBackground(views::CreateThemedRoundedRectBackground(kBackgroundColorId,
                                                         kRoundedCornerRadius));

  // Install and stylize the focus ring.
  StyleUtil::InstallRoundedCornerHighlightPathGenerator(
      this, gfx::RoundedCornersF(kRoundedCornerRadius));
  StyleUtil::SetUpFocusRingForView(this);
}

BirchChipButtonBase::~BirchChipButtonBase() = default;

BEGIN_METADATA(BirchChipButtonBase)
END_METADATA

}  // namespace ash
