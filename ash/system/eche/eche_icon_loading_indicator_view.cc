// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/eche/eche_icon_loading_indicator_view.h"
#include <algorithm>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_utils.h"
#include "base/scoped_observation.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

namespace {

constexpr int kThrobberStrokeWidth = 3;

}  // namespace

EcheIconLoadingIndicatorView::EcheIconLoadingIndicatorView(views::View* parent)
    : parent_(parent) {
  observed_session_.Observe(parent_);

  // Don't let the loading indicator process events.
  SetCanProcessEventsWithinSubtree(false);
}

EcheIconLoadingIndicatorView::~EcheIconLoadingIndicatorView() = default;

void EcheIconLoadingIndicatorView::SetAnimating(bool animating) {
  SetVisible(animating);

  if (animating && !throbber_start_time_.has_value()) {
    throbber_start_time_ = base::TimeTicks::Now();
    animation_.StartThrobbing(-1);
  } else {
    throbber_start_time_.reset();
    animation_.Reset();
  }
  OnPropertyChanged(&throbber_start_time_, views::kPropertyEffectsNone);
}

bool EcheIconLoadingIndicatorView::GetAnimating() const {
  return animation_.is_animating();
}

void EcheIconLoadingIndicatorView::OnPaint(gfx::Canvas* canvas) {
  if (!throbber_start_time_)
    return;

  // The image covers the container on the main axis and is centered on the
  // other axis. So we get the minimum of the height and width.
  int spinner_size_dip =
      std::min(GetLocalBounds().width(), GetLocalBounds().height());
  gfx::Rect bounds = GetLocalBounds();
  bounds.ClampToCenteredSize(gfx::Size(spinner_size_dip, spinner_size_dip));
  gfx::PaintThrobberSpinning(
      canvas, bounds,
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor),
      base::TimeTicks::Now() - *throbber_start_time_, kThrobberStrokeWidth);
}

void EcheIconLoadingIndicatorView::OnViewBoundsChanged(
    views::View* observed_view) {
  DCHECK_EQ(observed_view, parent_);
  SetBoundsRect(observed_view->GetLocalBounds());
}

void EcheIconLoadingIndicatorView::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, &animation_);
  SchedulePaint();
}

}  // namespace ash
