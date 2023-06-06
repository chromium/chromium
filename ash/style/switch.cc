// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/switch.h"

#include "ash/style/switch.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"

namespace ash {

namespace {

// Switch, track, and knob size.
constexpr int kSwitchWidth = 48;
constexpr int kSwitchHeight = 32;
constexpr int kSwitchInnerPadding = 8;
constexpr int kTrackInnerPadding = 2;
constexpr int kThumbRadius = 6;
constexpr int kFocusPadding = 2;

}  // namespace

//------------------------------------------------------------------------------
// Switch:

Switch::Switch(PressedCallback callback)
    : views::ToggleButton(callback, /*has_thumb_shadow=*/false) {
  SetBorder(views::CreateEmptyBorder(gfx::Insets(kSwitchInnerPadding)));
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
}

Switch::~Switch() = default;

gfx::Size Switch::CalculatePreferredSize() const {
  return gfx::Size(kSwitchWidth, kSwitchHeight);
}

SkPath Switch::GetFocusRingPath() const {
  gfx::Rect bounds = GetTrackBounds();
  const auto* focus_ring = views::FocusRing::Get(this);
  const int focus_ring_thickness =
      focus_ring ? focus_ring->GetHaloThickness()
                 : views::FocusRing::kDefaultHaloThickness;
  bounds.Inset(-gfx::Insets(kFocusPadding + focus_ring_thickness / 2));

  const SkScalar radius = SkIntToScalar(bounds.height() / 2);
  return SkPath::RRect(gfx::RectToSkRect(bounds), radius, radius);
}

gfx::Rect Switch::GetTrackBounds() const {
  return GetContentsBounds();
}

gfx::Rect Switch::GetThumbBounds() const {
  gfx::Rect bounds = GetTrackBounds();
  bounds.Inset(gfx::Insets(kTrackInnerPadding));

  const int thumb_size = 2 * kThumbRadius;
  bounds.set_x(bounds.x() +
               GetAnimationProgress() * (bounds.width() - thumb_size));
  bounds.set_size(gfx::Size(thumb_size, thumb_size));
  return bounds;
}

BEGIN_METADATA(Switch, views::ToggleButton)
END_METADATA

}  // namespace ash
