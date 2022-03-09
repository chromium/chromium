// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/eche/eche_icon_loading_indicator_view.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_utils.h"
#include "base/scoped_observation.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

namespace {

constexpr int kThrobberStrokeWidth = 2;

}  // namespace

EcheIconLoadingIndicatorView::EcheIconLoadingIndicatorView(
    views::ImageView* parent)
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

  const SkColor color =
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState());

  gfx::PaintThrobberSpinning(canvas, GetLocalBounds(), color,
                             base::TimeTicks::Now() - *throbber_start_time_,
                             kThrobberStrokeWidth);
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