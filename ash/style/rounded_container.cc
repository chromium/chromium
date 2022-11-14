// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/rounded_container.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// Rounded radius.
constexpr int kNonRoundedSideRadius = 4;
constexpr int kRoundedSideRadius = 16;

}  // namespace

RoundedContainer::RoundedContainer(Behavior corner_behavior)
    : corner_behavior_(corner_behavior) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetBackground(
      views::CreateThemedSolidBackground(cros_tokens::kCrosSysSystemOnBase));

  SetBorderInsets(kBorderInsets);

  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(GetRoundedCorners());
  layer()->SetFillsBoundsOpaquely(false);
}

RoundedContainer::~RoundedContainer() = default;

void RoundedContainer::SetBehavior(Behavior behavior) {
  corner_behavior_ = behavior;
  layer()->SetRoundedCornerRadius(GetRoundedCorners());
  SchedulePaint();
}

void RoundedContainer::SetBorderInsets(const gfx::Insets& insets) {
  SetBorder(views::CreateEmptyBorder(insets));
}

gfx::RoundedCornersF RoundedContainer::GetRoundedCorners() {
  switch (corner_behavior_) {
    case Behavior::kNotRounded:
      return {kNonRoundedSideRadius, kNonRoundedSideRadius,
              kNonRoundedSideRadius, kNonRoundedSideRadius};
    case Behavior::kAllRounded:
      return {kRoundedSideRadius, kRoundedSideRadius, kRoundedSideRadius,
              kRoundedSideRadius};
    case Behavior::kTopRounded:
      return {kRoundedSideRadius, kRoundedSideRadius, kNonRoundedSideRadius,
              kNonRoundedSideRadius};
    case Behavior::kBottomRounded:
      return {kNonRoundedSideRadius, kNonRoundedSideRadius, kRoundedSideRadius,
              kRoundedSideRadius};
  }
}

BEGIN_METADATA(RoundedContainer, views::View);
END_METADATA

}  // namespace ash
