// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/knob_switch.h"

#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

// Switch, track, and knob size.
constexpr int kSwitchWidth = 48;
constexpr int kSwitchHeight = 32;
constexpr int kSwitchInnerPadding = 8;
constexpr int kTrackInnerPadding = 2;
constexpr int kKnobRadius = 6;
constexpr int kFocusPadding = 2;

// Track and knob color ids.
constexpr ui::ColorId kSelectedTrackColorId = cros_tokens::kCrosSysPrimary;
constexpr ui::ColorId kSelectedKnobColorId = cros_tokens::kCrosSysOnPrimary;
constexpr ui::ColorId kUnSelectedTrackColorId = cros_tokens::kCrosSysSecondary;
constexpr ui::ColorId kUnSelectedKnobColorId = cros_tokens::kCrosSysOnSecondary;

}  // namespace

//------------------------------------------------------------------------------
// KnobSwitch:

KnobSwitch::KnobSwitch(KnobSwitch::Callback switch_callback)
    : switch_callback_(std::move(switch_callback)) {
  // Build view hierarchy. The track view and knob view cannot be focused and
  // process event.
  views::Builder<KnobSwitch>(this)
      .SetBorder(views::CreateEmptyBorder(gfx::Insets(kSwitchInnerPadding)))
      .SetPreferredSize(gfx::Size(kSwitchWidth, kSwitchHeight))
      .SetUseDefaultFillLayout(true)
      .AddChildren(
          views::Builder<views::View>()
              .CopyAddressTo(&track_)
              .SetFocusBehavior(views::View::FocusBehavior::NEVER)
              .SetPaintToLayer()
              .SetCanProcessEventsWithinSubtree(false)
              .SetBorder(
                  views::CreateEmptyBorder(gfx::Insets(kTrackInnerPadding)))
              .SetBackground(StyleUtil::CreateThemedFullyRoundedRectBackground(
                  kUnSelectedTrackColorId))
              .AddChildren(
                  views::Builder<views::View>()
                      .CopyAddressTo(&knob_)
                      .SetFocusBehavior(views::View::FocusBehavior::NEVER)
                      .SetPaintToLayer()
                      .SetCanProcessEventsWithinSubtree(false)
                      .SetPreferredSize(
                          gfx::Size(2 * kKnobRadius, 2 * kKnobRadius))
                      .SetBackground(
                          StyleUtil::CreateThemedFullyRoundedRectBackground(
                              kUnSelectedKnobColorId))))
      .BuildChildren();

  track_->layer()->SetFillsBoundsOpaquely(false);
  knob_->layer()->SetFillsBoundsOpaquely(false);

  // Install a pill shaped focus ring on the track.
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  const float halo_inset = focus_ring->GetHaloThickness() / 2.f + kFocusPadding;
  focus_ring->SetHaloInset(-halo_inset);
  auto pill_shape_path = std::make_unique<views::PillHighlightPathGenerator>();
  pill_shape_path->set_use_contents_bounds(true);
  views::HighlightPathGenerator::Install(this, std::move(pill_shape_path));

  // The switch is unselected initially.
  if (switch_callback_)
    switch_callback_.Run(false);
}

KnobSwitch::~KnobSwitch() = default;

void KnobSwitch::SetSelected(bool selected) {
  if (selected_ == selected)
    return;

  selected_ = selected;

  // Update the track and knob colors.
  const ui::ColorId knob_color_id =
      selected_ ? kSelectedKnobColorId : kUnSelectedKnobColorId;
  const ui::ColorId track_color_id =
      selected_ ? kSelectedTrackColorId : kUnSelectedTrackColorId;
  knob_->SetBackground(
      StyleUtil::CreateThemedFullyRoundedRectBackground(knob_color_id));
  track_->SetBackground(
      StyleUtil::CreateThemedFullyRoundedRectBackground(track_color_id));

  Layout();
  SchedulePaint();

  if (switch_callback_)
    switch_callback_.Run(selected_);
}

void KnobSwitch::Layout() {
  views::Button::Layout();

  // If selected, move the knob to the right. Otherwise, move knob to the left.
  const gfx::Rect track_contents_bounds = track_->GetContentsBounds();
  const int knob_x = selected_ ? track_contents_bounds.right() - 2 * kKnobRadius
                               : track_contents_bounds.x();
  const int knob_y = track_contents_bounds.y();
  knob_->SizeToPreferredSize();
  knob_->SetPosition(gfx::Point(knob_x, knob_y));
}

void KnobSwitch::StateChanged(ButtonState old_state) {
  if (GetState() == ButtonState::STATE_DISABLED) {
    track_->SetEnabled(false);
    knob_->SetEnabled(false);
  } else if (old_state == ButtonState::STATE_DISABLED) {
    track_->SetEnabled(true);
    knob_->SetEnabled(true);
  }
}

void KnobSwitch::NotifyClick(const ui::Event& event) {
  // Switch the current selected state on click.
  SetSelected(!selected_);
}

BEGIN_METADATA(KnobSwitch, views::View)
END_METADATA

}  // namespace ash
