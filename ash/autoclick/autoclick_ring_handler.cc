// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/autoclick_ring_handler.h"

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/transform.h"
#include "ui/views/view.h"

namespace ash {
namespace {

const int kAutoclickRingOuterRadius = 30;
const int kAutoclickRingInnerRadius = 20;

// Angles from x-axis at which the outer and inner circles start.
const int kAutoclickRingInnerStartAngle = -90;

const int kAutoclickRingGlowWidth = 20;
// The following is half width to avoid division by 2.
const int kAutoclickRingArcWidth = 2;

// Start and end values for various animations.
const double kAutoclickRingScaleStartValue = 1.0;
const double kAutoclickRingScaleEndValue = 1.0;
const double kAutoclickRingShrinkScaleEndValue = 0.5;

const double kAutoclickRingOpacityStartValue = 0.1;
const double kAutoclickRingOpacityEndValue = 0.5;
const int kAutoclickRingAngleStartValue = -90;
// The sweep angle is a bit greater than 360 to make sure the circle
// completes at the end of the animation.
const int kAutoclickRingAngleEndValue = 360;

// Visual constants.
const SkColor kAutoclickRingArcColor = SkColorSetARGB(255, 0, 255, 0);
const SkColor kAutoclickRingCircleColor = SkColorSetARGB(255, 0, 0, 255);

void PaintAutoclickRingCircle(gfx::Canvas* canvas,
                              gfx::Point& center,
                              int radius) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(2 * kAutoclickRingArcWidth);
  flags.setColor(kAutoclickRingCircleColor);
  flags.setAntiAlias(true);

  canvas->DrawCircle(center, radius, flags);
}

void PaintAutoclickRingArc(gfx::Canvas* canvas,
                           const gfx::Point& center,
                           int radius,
                           int start_angle,
                           int end_angle) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(2 * kAutoclickRingArcWidth);
  flags.setColor(kAutoclickRingArcColor);
  flags.setAntiAlias(true);

  SkPath arc_path;
  arc_path.addArc(SkRect::MakeXYWH(center.x() - radius, center.y() - radius,
                                   2 * radius, 2 * radius),
                  start_angle, end_angle - start_angle);
  canvas->DrawPath(arc_path, flags);
}
}  // namespace

// View of the AutoclickRingHandler. Draws the actual contents and updates as
// the animation proceeds. It also maintains the views::Widget that the
// animation is shown in.
class AutoclickRingHandler::AutoclickRingView : public views::View {
 public:
  AutoclickRingView(const gfx::Point& event_location,
                    views::Widget* ring_widget)
      : views::View(),
        widget_(ring_widget),
        current_angle_(kAutoclickRingAngleStartValue),
        current_scale_(kAutoclickRingScaleStartValue) {
    widget_->SetContentsView(this);

    // We are owned by the AutoclickRingHandler.
    set_owned_by_client();
    SetNewLocation(event_location);
  }

  ~AutoclickRingView() override = default;

  void SetNewLocation(const gfx::Point& new_event_location) {
    gfx::Point point = new_event_location;
    widget_->SetBounds(gfx::Rect(
        point.x() - (kAutoclickRingOuterRadius + kAutoclickRingGlowWidth),
        point.y() - (kAutoclickRingOuterRadius + kAutoclickRingGlowWidth),
        GetPreferredSize().width(), GetPreferredSize().height()));
    widget_->Show();
    widget_->GetNativeView()->layer()->SetOpacity(
        kAutoclickRingOpacityStartValue);
  }

  void UpdateWithGrowAnimation(gfx::Animation* animation) {
    // Update the portion of the circle filled so far and re-draw.
    current_angle_ = animation->CurrentValueBetween(
        kAutoclickRingInnerStartAngle, kAutoclickRingAngleEndValue);
    current_scale_ = animation->CurrentValueBetween(
        kAutoclickRingScaleStartValue, kAutoclickRingScaleEndValue);
    widget_->GetNativeView()->layer()->SetOpacity(
        animation->CurrentValueBetween(kAutoclickRingOpacityStartValue,
                                       kAutoclickRingOpacityEndValue));
    SchedulePaint();
  }

  void UpdateWithShrinkAnimation(gfx::Animation* animation) {
    current_angle_ = animation->CurrentValueBetween(
        kAutoclickRingInnerStartAngle, kAutoclickRingAngleEndValue);
    current_scale_ = animation->CurrentValueBetween(
        kAutoclickRingScaleEndValue, kAutoclickRingShrinkScaleEndValue);
    widget_->GetNativeView()->layer()->SetOpacity(
        animation->CurrentValueBetween(kAutoclickRingOpacityStartValue,
                                       kAutoclickRingOpacityEndValue));
    SchedulePaint();
  }

 private:
  // Overridden from views::View.
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(2 * (kAutoclickRingOuterRadius + kAutoclickRingGlowWidth),
                     2 * (kAutoclickRingOuterRadius + kAutoclickRingGlowWidth));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Point center(GetPreferredSize().width() / 2,
                      GetPreferredSize().height() / 2);
    canvas->Save();

    gfx::Transform scale;
    scale.Scale(current_scale_, current_scale_);
    // We want to scale from the center.
    canvas->Translate(center.OffsetFromOrigin());
    canvas->Transform(scale);
    canvas->Translate(-center.OffsetFromOrigin());

    // Paint inner circle.
    PaintAutoclickRingArc(canvas, center, kAutoclickRingInnerRadius,
                          kAutoclickRingInnerStartAngle, current_angle_);
    // Paint outer circle.
    PaintAutoclickRingCircle(canvas, center, kAutoclickRingOuterRadius);

    canvas->Restore();
  }

  views::Widget* widget_;
  int current_angle_;
  double current_scale_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickRingView);
};

////////////////////////////////////////////////////////////////////////////////

// AutoclickRingHandler, public
AutoclickRingHandler::AutoclickRingHandler()
    : gfx::LinearAnimation(nullptr),
      ring_widget_(nullptr),
      current_animation_type_(AnimationType::NONE) {}

AutoclickRingHandler::~AutoclickRingHandler() {
  StopAutoclickRing();
}

void AutoclickRingHandler::StartGesture(
    base::TimeDelta duration,
    const gfx::Point& center_point_in_screen,
    views::Widget* widget) {
  StopAutoclickRing();
  tap_down_location_ = center_point_in_screen;
  ring_widget_ = widget;
  current_animation_type_ = AnimationType::GROW_ANIMATION;
  animation_duration_ = duration;
  StartAnimation(base::TimeDelta());
}

void AutoclickRingHandler::StopGesture() {
  StopAutoclickRing();
}

void AutoclickRingHandler::SetGestureCenter(
    const gfx::Point& center_point_in_screen,
    views::Widget* widget) {
  tap_down_location_ = center_point_in_screen;
  ring_widget_ = widget;
}
////////////////////////////////////////////////////////////////////////////////

// AutoclickRingHandler, private
void AutoclickRingHandler::StartAnimation(base::TimeDelta delay) {
  switch (current_animation_type_) {
    case AnimationType::GROW_ANIMATION: {
      view_.reset(new AutoclickRingView(tap_down_location_, ring_widget_));
      SetDuration(delay);
      Start();
      break;
    }
    case AnimationType::SHRINK_ANIMATION: {
      view_.reset(new AutoclickRingView(tap_down_location_, ring_widget_));
      SetDuration(delay);
      Start();
      break;
    }
    case AnimationType::NONE:
      NOTREACHED();
      break;
  }
}

void AutoclickRingHandler::StopAutoclickRing() {
  // Since, Animation::Stop() calls AnimationStopped(), we need to reset the
  // |current_animation_type_| before Stop(), otherwise AnimationStopped() may
  // start the timer again.
  current_animation_type_ = AnimationType::NONE;
  Stop();
  view_.reset();
}

void AutoclickRingHandler::AnimateToState(double state) {
  DCHECK(view_.get());
  switch (current_animation_type_) {
    case AnimationType::GROW_ANIMATION:
      view_->SetNewLocation(tap_down_location_);
      view_->UpdateWithGrowAnimation(this);
      break;
    case AnimationType::SHRINK_ANIMATION:
      view_->SetNewLocation(tap_down_location_);
      view_->UpdateWithShrinkAnimation(this);
      break;
    case AnimationType::NONE:
      NOTREACHED();
      break;
  }
}

void AutoclickRingHandler::AnimationStopped() {
  switch (current_animation_type_) {
    case AnimationType::GROW_ANIMATION:
      current_animation_type_ = AnimationType::SHRINK_ANIMATION;
      StartAnimation(animation_duration_);
      break;
    case AnimationType::SHRINK_ANIMATION:
      current_animation_type_ = AnimationType::NONE;
      break;
    case AnimationType::NONE:
      // fall through to reset the view.
      view_.reset();
      break;
  }
}

}  // namespace ash
