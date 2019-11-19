// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_view.h"

#include <memory>

#include "ash/highlighter/highlighter_gesture_util.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Variables for rendering the highlighter. Sizes in DIP.
const SkColor kHighlighterColor = SkColorSetRGB(0x42, 0x85, 0xF4);
constexpr int kHighlighterOpacity = 0xCC;
constexpr float kPenTipWidth = 4;
constexpr float kPenTipHeight = 14;
constexpr int kOutsetForAntialiasing = 1;

constexpr float kStrokeScale = 1.2;

constexpr int kStrokeFadeoutDelayMs = 100;
constexpr int kStrokeFadeoutDurationMs = 500;
constexpr int kStrokeScaleDurationMs = 300;

gfx::Rect InflateDamageRect(const gfx::Rect& r) {
  gfx::Rect inflated = r;
  inflated.Inset(
      -kOutsetForAntialiasing - static_cast<int>(kPenTipWidth / 2 + 1),
      -kOutsetForAntialiasing - static_cast<int>(kPenTipHeight / 2 + 1));
  return inflated;
}

// A highlighter segment is a parallelogram with two vertical sides.
// |p1| and |p2| are the center points of the vertical sides.
// |height| is the height of a vertical side.
void DrawSegment(gfx::Canvas& canvas,
                 const gfx::PointF& p1,
                 const gfx::PointF& p2,
                 int height,
                 const cc::PaintFlags& flags) {
  const float y_offset = height / 2;
  SkPath path;
  // When drawn with a thick round-joined outline, starting in the middle
  // of a vertical edge ensures smooth joining with the last edge.
  path.moveTo(p1.x(), p1.y());
  path.lineTo(p1.x(), p1.y() - y_offset);
  path.lineTo(p2.x(), p2.y() - y_offset);
  path.lineTo(p2.x(), p2.y() + y_offset);
  path.lineTo(p1.x(), p1.y() + y_offset);
  path.lineTo(p1.x(), p1.y() - y_offset);
  path.lineTo(p1.x(), p1.y());
  canvas.DrawPath(path, flags);
}

}  // namespace

const SkColor HighlighterView::kPenColor =
    SkColorSetA(kHighlighterColor, kHighlighterOpacity);

const gfx::SizeF HighlighterView::kPenTipSize(kPenTipWidth, kPenTipHeight);

HighlighterView::HighlighterView(base::TimeDelta presentation_delay,
                                 aura::Window* container)
    : FastInkView(container, PresentationCallback()),
      points_(base::TimeDelta()),
      predicted_points_(base::TimeDelta()),
      presentation_delay_(presentation_delay) {}

HighlighterView::~HighlighterView() = default;

void HighlighterView::AddNewPoint(const gfx::PointF& point,
                                  const base::TimeTicks& time) {
  TRACE_EVENT1("ui", "HighlighterView::AddNewPoint", "point", point.ToString());

  TRACE_COUNTER1(
      "ui", "HighlighterPredictionError",
      predicted_points_.GetNumberOfPoints()
          ? std::round(
                (point - predicted_points_.GetOldest().location).Length())
          : 0);
  // The new segment needs to be drawn.
  if (!points_.IsEmpty()) {
    highlighter_damage_rect_.Union(InflateDamageRect(gfx::ToEnclosingRect(
        gfx::BoundingRect(points_.GetNewest().location, point))));
  }

  // Previous prediction needs to be erased.
  if (!predicted_points_.IsEmpty()) {
    highlighter_damage_rect_.Union(
        InflateDamageRect(predicted_points_.GetBoundingBox()));
  }

  points_.AddPoint(point, time);

  base::TimeTicks current_time = ui::EventTimeForNow();
  gfx::Rect screen_bounds = GetWidget()->GetNativeView()->GetBoundsInScreen();
  predicted_points_.Predict(points_, current_time, presentation_delay_,
                            screen_bounds.size());

  // New prediction needs to be drawn.
  if (!predicted_points_.IsEmpty()) {
    highlighter_damage_rect_.Union(
        InflateDamageRect(predicted_points_.GetBoundingBox()));
  }

  ScheduleUpdateBuffer();
}

void HighlighterView::AddGap() {
  points_.AddGap();
}

void HighlighterView::Animate(const gfx::PointF& pivot,
                              HighlighterGestureType gesture_type,
                              const base::Closure& done) {
  animation_timer_ = std::make_unique<base::OneShotTimer>();
  animation_timer_->Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kStrokeFadeoutDelayMs),
      base::BindRepeating(&HighlighterView::FadeOut, base::Unretained(this),
                          pivot, gesture_type, done));
}

void HighlighterView::FadeOut(const gfx::PointF& pivot,
                              HighlighterGestureType gesture_type,
                              const base::Closure& done) {
  ui::Layer* layer = GetWidget()->GetLayer();

  base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(kStrokeFadeoutDurationMs);

  {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(duration);
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);

    layer->SetOpacity(0);
  }

  if (gesture_type != HighlighterGestureType::kHorizontalStroke) {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kStrokeScaleDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);

    const float scale = gesture_type == HighlighterGestureType::kClosedShape
                            ? kStrokeScale
                            : (1 / kStrokeScale);

    gfx::Transform transform;
    transform.Translate(pivot.x() * (1 - scale), pivot.y() * (1 - scale));
    transform.Scale(scale, scale);

    layer->SetTransform(transform);
  }

  animation_timer_ = std::make_unique<base::OneShotTimer>();
  animation_timer_->Start(FROM_HERE, duration, done);
}

void HighlighterView::ScheduleUpdateBuffer() {
  if (pending_update_buffer_)
    return;

  pending_update_buffer_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&HighlighterView::UpdateBuffer,
                                weak_ptr_factory_.GetWeakPtr()));
}

void HighlighterView::UpdateBuffer() {
  TRACE_EVENT1("ui", "FastInkView::UpdateBuffer", "damage",
               highlighter_damage_rect_.ToString());

  DCHECK(pending_update_buffer_);
  pending_update_buffer_ = false;

  {
    ScopedPaint paint(gpu_memory_buffer_.get(), screen_to_buffer_transform_,
                      highlighter_damage_rect_);

    Draw(paint.canvas());
  }

  gfx::Rect screen_bounds = GetWidget()->GetNativeView()->GetBoundsInScreen();
  UpdateSurface(screen_bounds, highlighter_damage_rect_, /*auto_refresh=*/true);
  highlighter_damage_rect_ = gfx::Rect();
}

void HighlighterView::Draw(gfx::Canvas& canvas) {
  const int num_points =
      points_.GetNumberOfPoints() + predicted_points_.GetNumberOfPoints();
  if (num_points < 2)
    return;

  gfx::Rect clip_rect;
  if (!canvas.GetClipBounds(&clip_rect))
    return;

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStrokeAndFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(kPenColor);
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setStrokeWidth(kPenTipWidth);
  flags.setStrokeJoin(cc::PaintFlags::kRound_Join);

  // Decrease the segment height by the outline stroke width,
  // so that the vertical cross-section of the drawn segment
  // is exactly kPenTipHeight.
  const int height = kPenTipHeight - kPenTipWidth;

  fast_ink::FastInkPoints::FastInkPoint previous_point;

  for (int i = 0; i < num_points; ++i) {
    fast_ink::FastInkPoints::FastInkPoint current_point;
    if (i < points_.GetNumberOfPoints()) {
      current_point = points_.points()[i];
    } else {
      current_point =
          predicted_points_.points()[i - points_.GetNumberOfPoints()];
    }

    if (i != 0 && !previous_point.gap_after) {
      gfx::Rect damage_rect = InflateDamageRect(gfx::ToEnclosingRect(
          gfx::BoundingRect(previous_point.location, current_point.location)));
      // Only draw the segment if it is touching the clip rect.
      if (clip_rect.Intersects(damage_rect)) {
        DrawSegment(canvas, previous_point.location, current_point.location,
                    height, flags);
      }
    }

    previous_point = current_point;
  }
}

}  // namespace ash
