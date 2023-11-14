// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/eche/eche_icon_loading_indicator_view.h"
#include <algorithm>

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

namespace {

constexpr int kThrobberStrokeWidth = 3;

}  // namespace

EcheIconLoadingIndicatorView::EcheIconLoadingIndicatorView(views::View* parent)
    : parent_(parent) {
  observed_session_.Observe(parent_.get());

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
      canvas, bounds, GetColorProvider()->GetColor(ui::kColorAshFocusRing),
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

BEGIN_METADATA(EcheIconLoadingIndicatorView)
END_METADATA

}  // namespace ash
