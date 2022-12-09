// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/knob_switch.h"

#include "ash/style/color_util.h"
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

//------------------------------------------------------------------------------
// ThemedFullyRoundedRectBackground
// A themed fully rounded rect background whose corner radius equals to the half
// of the minimum dimension of its view's local bounds.

class ThemedFullyRoundedRectBackground : public views::Background {
 public:
  explicit ThemedFullyRoundedRectBackground(ui::ColorId color_id)
      : color_id_(color_id) {}

  ThemedFullyRoundedRectBackground(const ThemedFullyRoundedRectBackground&) =
      delete;
  ThemedFullyRoundedRectBackground& operator=(
      const ThemedFullyRoundedRectBackground&) = delete;
  ~ThemedFullyRoundedRectBackground() override = default;

  static std::unique_ptr<Background> Create(ui::ColorId color_id) {
    return std::make_unique<ThemedFullyRoundedRectBackground>(color_id);
  }

  void OnViewThemeChanged(views::View* view) override {
    SetNativeControlColor(view->GetColorProvider()->GetColor(color_id_));
    view->SchedulePaint();
  }

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    // Draw a fully rounded rect filling in the view's local bounds.
    cc::PaintFlags paint;
    paint.setAntiAlias(true);

    SkColor color = get_color();
    if (!view->GetEnabled()) {
      color = ColorUtil::GetDisabledColor(color);
    }
    paint.setColor(color);

    const gfx::Rect bounds = view->GetLocalBounds();
    const int radius = std::min(bounds.width(), bounds.height()) / 2;
    canvas->DrawRoundRect(bounds, radius, paint);
  }

 private:
  // Color Id of the background.
  ui::ColorId color_id_;
};

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
              .SetBackground(ThemedFullyRoundedRectBackground::Create(
                  kUnSelectedTrackColorId))
              .AddChildren(
                  views::Builder<views::View>()
                      .CopyAddressTo(&knob_)
                      .SetFocusBehavior(views::View::FocusBehavior::NEVER)
                      .SetPaintToLayer()
                      .SetCanProcessEventsWithinSubtree(false)
                      .SetPreferredSize(
                          gfx::Size(2 * kKnobRadius, 2 * kKnobRadius))
                      .SetBackground(ThemedFullyRoundedRectBackground::Create(
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
  knob_->SetBackground(ThemedFullyRoundedRectBackground::Create(knob_color_id));
  track_->SetBackground(
      ThemedFullyRoundedRectBackground::Create(track_color_id));

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
