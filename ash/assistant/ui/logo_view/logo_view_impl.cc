// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/logo_view/logo_view_impl.h"

#include <algorithm>

#include "ash/assistant/ui/logo_view/shape/shape.h"
#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/notreached.h"
#include "chromeos/assistant/internal/logo_view/logo_model/dot.h"
#include "chromeos/assistant/internal/logo_view/logo_view_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ash {

namespace {

int64_t TimeTicksToMs(const base::TimeTicks& timestamp) {
  return (timestamp - base::TimeTicks()).InMilliseconds();
}

}  // namespace

LogoViewImpl::LogoViewImpl()
    : mic_part_shape_(chromeos::assistant::kDotDefaultSize),
      state_animator_(&logo_, &state_model_, StateModel::State::kUndefined) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  state_animator_.SetStateAnimatorTimerDelegate(this);
  state_animator_.SetLogoInputValueProvider(StateModel::State::kUserSpeaks,
                                            &sound_level_input_value_provider_);
}

LogoViewImpl::~LogoViewImpl() {
  state_animator_.StopAnimator();
}

void LogoViewImpl::SetState(LogoView::State state, bool animate) {
  StateModel::State animator_state;
  switch (state) {
    case LogoView::State::kUndefined:
      animator_state = StateModel::State::kUndefined;
      break;
    case LogoView::State::kListening:
      animator_state = StateModel::State::kListening;
      break;
    case LogoView::State::kMic:
      animator_state = StateModel::State::kMic;
      break;
    case LogoView::State::kUserSpeaks:
      animator_state = StateModel::State::kUserSpeaks;
      break;
  }
  state_animator_.SwitchStateTo(animator_state, !animate);
}

void LogoViewImpl::SetSpeechLevel(float speech_level) {
  sound_level_input_value_provider_.SetSpeechLevel(speech_level);
}

int64_t LogoViewImpl::StartTimer() {
  // Remove animation observer from previous |StartTimer| if exists.
  StopTimer();

  ui::Compositor* compositor = layer()->GetCompositor();
  if (compositor && !compositor->HasAnimationObserver(this)) {
    animating_compositor_ = compositor;
    animating_compositor_->AddAnimationObserver(this);
  }
  return TimeTicksToMs(base::TimeTicks::Now());
}

void LogoViewImpl::StopTimer() {
  if (animating_compositor_ &&
      animating_compositor_->HasAnimationObserver(this)) {
    animating_compositor_->RemoveAnimationObserver(this);
  }
  animating_compositor_ = nullptr;
}

void LogoViewImpl::OnAnimationStep(base::TimeTicks timestamp) {
  const int64_t current_time_ms = TimeTicksToMs(timestamp);
  state_animator_.OnTimeUpdate(current_time_ms);
  SchedulePaint();
}

void LogoViewImpl::OnCompositingShuttingDown(ui::Compositor* compositor) {
  DCHECK(compositor);
  if (animating_compositor_ == compositor) {
    StopTimer();
  }
}

void LogoViewImpl::DrawDots(gfx::Canvas* canvas) {
  // TODO: The Green Mic parts seems overlapped on the Red Mic part. Draw dots
  // in reverse order so that the Red Mic part is on top of Green Mic parts. But
  // we need to find out why the Mic parts are overlapping in the first place.
  for (const auto& dot : base::Reversed(logo_.dots())) {
    DrawDot(canvas, dot.get());
  }
}

void LogoViewImpl::DrawDot(gfx::Canvas* canvas, Dot* dot) {
  const float radius = dot->GetRadius();
  const float angle = logo_.GetRotation() + dot->GetAngle();
  const float x = radius * std::cos(angle) + dot->GetOffsetX();
  const float y = radius * std::sin(angle) + dot->GetOffsetY();

  if (dot->IsMic()) {
    DrawMicPart(canvas, dot, x, y);
  } else if (dot->IsLetter()) {
    // TODO(b/79579731): Implement the letter animation.
    NOTIMPLEMENTED();
  } else if (dot->IsLine()) {
    DrawLine(canvas, dot, x, y);
  } else {
    DrawCircle(canvas, dot, x, y);
  }
}

void LogoViewImpl::DrawMicPart(gfx::Canvas* canvas,
                               Dot* dot,
                               float x,
                               float y) {
  const float progress = dot->GetMicMorph();
  mic_part_shape_.Reset();
  mic_part_shape_.ToMicPart(progress, dot->dot_color());
  mic_part_shape_.Transform(x, y, dots_scale_);
  DrawShape(canvas, &mic_part_shape_, dot->color());
}

void LogoViewImpl::DrawShape(gfx::Canvas* canvas, Shape* shape, SkColor color) {
  cc::PaintFlags paint_flags;
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(color);
  paint_flags.setAlphaf(logo_.GetAlpha());
  paint_flags.setStyle(cc::PaintFlags::kStroke_Style);
  paint_flags.setStrokeCap(shape->cap());

  paint_flags.setStrokeWidth(shape->first_stroke_width());
  canvas->DrawPath(*shape->first_path(), paint_flags);

  paint_flags.setStrokeWidth(shape->second_stroke_width());
  canvas->DrawPath(*shape->second_path(), paint_flags);
}

void LogoViewImpl::DrawLine(gfx::Canvas* canvas, Dot* dot, float x, float y) {
  const float stroke_width = dot->GetSize() * dots_scale_;
  cc::PaintFlags paint_flags;
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(dot->color());
  paint_flags.setAlphaf(logo_.GetAlpha());
  paint_flags.setStrokeWidth(stroke_width);
  paint_flags.setStyle(cc::PaintFlags::kStroke_Style);
  paint_flags.setStrokeCap(cc::PaintFlags::kRound_Cap);

  const float line_length = dot->GetLineLength();
  const float line_x = x * dots_scale_;
  const float line_top_y = (y - line_length) * dots_scale_;
  const float line_bottom_y = (y + line_length) * dots_scale_;
  canvas->DrawLine(gfx::PointF(line_x, line_top_y),
                   gfx::PointF(line_x, line_bottom_y), paint_flags);
}

void LogoViewImpl::DrawCircle(gfx::Canvas* canvas, Dot* dot, float x, float y) {
  const float radius = dot->GetSize() * dot->GetVisibility() / 2.0f;
  cc::PaintFlags paint_flags;
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(dot->color());
  paint_flags.setAlphaf(logo_.GetAlpha());
  paint_flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::PointF(x * dots_scale_, y * dots_scale_),
                     radius * dots_scale_, paint_flags);
}

void LogoViewImpl::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  canvas->Save();
  gfx::RectF content_bounds(GetContentsBounds());
  gfx::InsetsF insets(GetInsets());
  const float offset_x = insets.left() + content_bounds.width() / 2.0f;
  const float offset_y = insets.top() + content_bounds.height() / 2.0f;
  canvas->Translate(gfx::Vector2d(offset_x, offset_y));
  DrawDots(canvas);
  canvas->Restore();
}

void LogoViewImpl::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  gfx::Rect content_bounds(GetContentsBounds());
  if (content_bounds.IsEmpty()) {
    return;
  }

  // Sets a scale such that an object of the specified width and height will
  // fill the view while keeping the aspect ratio if drawn at that scale.
  constexpr float kDefaultWidth = 28.0f;
  constexpr float kDefaultHeight = 25.0f;
  const float x_scale = content_bounds.width() / kDefaultWidth;
  const float y_scale = content_bounds.height() / kDefaultHeight;
  dots_scale_ = std::fmin(x_scale, y_scale);
}

void LogoViewImpl::VisibilityChanged(views::View* starting_from,
                                     bool is_visible) {
  if (IsDrawn()) {
    state_animator_.StartAnimator();
  } else {
    state_animator_.StopAnimator();
  }
}

BEGIN_METADATA(LogoViewImpl)
END_METADATA

}  // namespace ash
