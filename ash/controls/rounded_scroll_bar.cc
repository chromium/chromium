// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/controls/rounded_scroll_bar.h"

#include <limits>

#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_id.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"

namespace ash {
namespace {

// Thickness of scroll bar thumb.
constexpr int kScrollThumbThicknessDp = 8;
constexpr int kScrollThumbThicknessHoverInsets = 2;
constexpr int kScrollThumbOutlineTickness = 1;
// How long for the scrollbar to hide after no scroll events have been received?
constexpr base::TimeDelta kScrollThumbHideTimeout = base::Milliseconds(500);
// How long for the scrollbar to fade away?
constexpr base::TimeDelta kScrollThumbFadeDuration = base::Milliseconds(240);
// Opacity values from go/semantic-color-system for "Scrollbar".
constexpr float kDefaultOpacity = 0.38f;
constexpr float kActiveOpacity = 1.0f;

// The active state is when the thumb is hovered or pressed.
bool IsActiveState(views::Button::ButtonState state) {
  return state == views::Button::STATE_HOVERED ||
         state == views::Button::STATE_PRESSED;
}

// Draws a fully rounded rectangle filling in the given bounds.
void DrawFullyRoundedRect(gfx::Canvas* canvas,
                          const gfx::RectF& bounds,
                          const cc::PaintFlags& flags) {
  const SkScalar corner_radius = std::min(bounds.width(), bounds.height()) / 2;
  SkPath rounded_rect;
  rounded_rect.addRoundRect(gfx::RectFToSkRect(bounds), corner_radius,
                            corner_radius);
  canvas->DrawPath(rounded_rect, flags);
}

}  // namespace

// A scroll bar "thumb" that paints itself with rounded ends.
class RoundedScrollBar::Thumb : public views::BaseScrollBarThumb {
 public:
  explicit Thumb(RoundedScrollBar* scroll_bar)
      : BaseScrollBarThumb(scroll_bar), scroll_bar_(scroll_bar) {}
  Thumb(const Thumb&) = delete;
  Thumb& operator=(const Thumb&) = delete;
  ~Thumb() override = default;

  bool ShouldPaintAsActive() const {
    // The thumb is active during hover and also when the user is dragging the
    // thumb with the mouse. In the latter case, the mouse might be outside the
    // scroll bar, due to mouse capture.
    return IsActiveState(GetState());
  }

  int GetThumbThickness() const {
    if (!chromeos::features::IsJellyrollEnabled() || ShouldPaintAsActive()) {
      return kScrollThumbThicknessDp;
    }
    return kScrollThumbThicknessDp - kScrollThumbThicknessHoverInsets;
  }

  // views::BaseScrollBarThumb:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& /*available_size*/) const override {
    const int thickness = GetThumbThickness();
    return gfx::Size(thickness, thickness);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    // Scale bounds with device scale factor to make sure border bounds match
    // thumb bounds.
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float dsf = canvas->UndoDeviceScaleFactor();
    const gfx::RectF local_bounds =
        gfx::ConvertRectToPixels(GetLocalBounds(), dsf);
    gfx::RectF thumb_bounds(local_bounds);

    // Can be nullptr in tests.
    auto* color_provider = GetColorProvider();

    const bool is_jellyroll_enabled = chromeos::features::IsJellyrollEnabled();
    if (is_jellyroll_enabled) {
      // Paint outline.
      cc::PaintFlags stroke_flags;
      stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
      if (color_provider) {
        stroke_flags.setColor(
            color_provider->GetColor(cros_tokens::kCrosSysScrollbarBorder));
      }
      stroke_flags.setStrokeWidth(kScrollThumbOutlineTickness);
      stroke_flags.setAntiAlias(true);

      gfx::RectF border_bounds = local_bounds;
      border_bounds.Inset(kScrollThumbOutlineTickness / 2.0f);

      DrawFullyRoundedRect(canvas, border_bounds, stroke_flags);

      thumb_bounds.Inset(kScrollThumbOutlineTickness);
    }

    // Paint thumb.
    cc::PaintFlags fill_flags;
    fill_flags.setStyle(cc::PaintFlags::kFill_Style);
    fill_flags.setAntiAlias(true);
    if (color_provider) {
      fill_flags.setColor(color_provider->GetColor(
          is_jellyroll_enabled
              ? (ShouldPaintAsActive() ? cros_tokens::kCrosSysScrollbarHover
                                       : cros_tokens::kCrosSysScrollbar)
              : static_cast<ui::ColorId>(kColorAshScrollBarColor)));
    }

    DrawFullyRoundedRect(canvas, thumb_bounds, fill_flags);
  }

  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    scroll_bar_->OnThumbBoundsChanged();
  }

  void OnStateChanged() override {
    scroll_bar_->OnThumbStateChanged(current_state_);
    current_state_ = GetState();
  }

 private:
  const raw_ptr<RoundedScrollBar> scroll_bar_;
  views::Button::ButtonState current_state_ = views::Button::STATE_NORMAL;
};

RoundedScrollBar::RoundedScrollBar(Orientation orientation)
    : ScrollBar(orientation),
      thumb_(new Thumb(this)),  // Owned by views hierarchy.
      hide_scrollbar_timer_(
          FROM_HERE,
          kScrollThumbHideTimeout,
          base::BindRepeating(&RoundedScrollBar::HideScrollBar,
                              base::Unretained(this))) {
  // Moving the mouse directly into the thumb will also notify this view.
  SetNotifyEnterExitOnChild(true);

  SetThumb(thumb_);
  thumb_->SetPaintToLayer();
  thumb_->layer()->SetFillsBoundsOpaquely(false);
  // The thumb is hidden by default.
  thumb_->layer()->SetOpacity(0.f);
}

RoundedScrollBar::~RoundedScrollBar() = default;

void RoundedScrollBar::SetInsets(const gfx::Insets& insets) {
  insets_ = insets;
}

void RoundedScrollBar::SetSnapBackOnDragOutside(bool snap) {
  thumb_->SetSnapBackOnDragOutside(snap);
}

void RoundedScrollBar::SetShowOnThumbBoundsChanged(bool show) {
  show_on_thumb_bounds_changed_ = show;
}

gfx::Rect RoundedScrollBar::GetTrackBounds() const {
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(insets_);
  return bounds;
}

bool RoundedScrollBar::OverlapsContent() const {
  return true;
}

int RoundedScrollBar::GetThickness() const {
  // Extend the thickness by the insets on the sides of the bar.
  const int sides = GetOrientation() == Orientation::kHorizontal
                        ? insets_.top() + insets_.bottom()
                        : insets_.left() + insets_.right();
  return thumb_->GetThumbThickness() + sides;
}

void RoundedScrollBar::OnMouseEntered(const ui::MouseEvent& event) {
  ShowScrollbar();
}

void RoundedScrollBar::OnMouseExited(const ui::MouseEvent& event) {
  if (!hide_scrollbar_timer_.IsRunning() && !always_show_thumb_) {
    hide_scrollbar_timer_.Reset();
  }
}

void RoundedScrollBar::ScrollToPosition(int position) {
  ShowScrollbar();
  views::ScrollBar::ScrollToPosition(position);
}

void RoundedScrollBar::ObserveScrollEvent(const ui::ScrollEvent& event) {
  // Scroll fling events are generated by moving a single finger over the
  // trackpad; do not show the scrollbar for these events.
  if (event.type() == ui::EventType::kScrollFlingCancel) {
    return;
  }
  ShowScrollbar();
}

void RoundedScrollBar::SetAlwaysShowThumb(bool always_show_thumb) {
  always_show_thumb_ = always_show_thumb;

  if (always_show_thumb_) {
    hide_scrollbar_timer_.Stop();
    ShowScrollbar();
    return;
  }

  hide_scrollbar_timer_.Reset();
}

views::BaseScrollBarThumb* RoundedScrollBar::GetThumbForTest() const {
  return thumb_;
}

void RoundedScrollBar::ShowScrollbar() {
  if (!IsMouseHovered() && !always_show_thumb_) {
    hide_scrollbar_timer_.Reset();
  }

  const float target_opacity = (chromeos::features::IsJellyrollEnabled() ||
                                thumb_->ShouldPaintAsActive())
                                   ? kActiveOpacity
                                   : kDefaultOpacity;
  if (base::IsApproximatelyEqual(thumb_->layer()->GetTargetOpacity(),
                                 target_opacity,
                                 std::numeric_limits<float>::epsilon())) {
    return;
  }
  ui::ScopedLayerAnimationSettings animation(thumb_->layer()->GetAnimator());
  animation.SetTransitionDuration(kScrollThumbFadeDuration);
  thumb_->layer()->SetOpacity(target_opacity);
}

void RoundedScrollBar::HideScrollBar() {
  // Never hide the scrollbar if the mouse is over it. The auto-hide timer
  // will be reset when the mouse leaves the scrollable area.
  if (IsMouseHovered() || always_show_thumb_) {
    return;
  }

  hide_scrollbar_timer_.Stop();
  ui::ScopedLayerAnimationSettings animation(thumb_->layer()->GetAnimator());
  animation.SetTransitionDuration(kScrollThumbFadeDuration);
  thumb_->layer()->SetOpacity(0.f);
}

void RoundedScrollBar::OnThumbStateChanged(
    views::Button::ButtonState old_state) {
  // Update the scroll bar track and thumb bounds as needed. This won't
  // re-layout the scroll contents since the scroll bar overlaps the contents.
  if (chromeos::features::IsJellyrollEnabled() &&
      IsActiveState(old_state) != thumb_->ShouldPaintAsActive()) {
    PreferredSizeChanged();
  }

  // If the mouse is still in the scroll bar, the thumb hover state may have
  // changed, so recompute opacity.
  if (IsMouseHovered()) {
    ShowScrollbar();
  }
}

void RoundedScrollBar::OnThumbBoundsChanged() {
  // Optionally show the scroll bar on thumb bounds changes (e.g. keyboard
  // driven scroll position changes).
  if (show_on_thumb_bounds_changed_) {
    ShowScrollbar();
  }
}

BEGIN_METADATA(RoundedScrollBar)
END_METADATA

}  // namespace ash
