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

// The default values of the autoclick ring widget size.
const int kAutoclickRingInnerRadius = 20;

// The following is half width to avoid division by 2.
const int kAutoclickRingArcWidth = 2;

const int kAutoclickRingAngleStartValue = -90;
// The sweep angle is a bit greater than 360 to make sure the circle
// completes at the end of the animation.
const int kAutoclickRingAngleEndValue = 360;

// Constants for colors.
const SkColor kAutoclickRingArcColor = SkColorSetARGB(255, 255, 255, 255);
const SkColor kAutoclickRingCircleColor = SkColorSetARGB(128, 0, 0, 0);
const SkColor kAutoclickRingUnderArcColor = SkColorSetARGB(100, 128, 134, 139);

// Paints the full Autoclick ring.
void PaintAutoclickRing(gfx::Canvas* canvas,
                        const gfx::Point& center,
                        int radius,
                        int end_angle) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  // Draw the point being selected.
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(kAutoclickRingArcColor);
  canvas->DrawCircle(center, kAutoclickRingArcWidth / 2, flags);

  // Draw the outline of the point being selected.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kAutoclickRingArcWidth);
  flags.setColor(kAutoclickRingCircleColor);
  canvas->DrawCircle(center, kAutoclickRingArcWidth * 3 / 2, flags);

  // Draw the outline of the arc.
  flags.setColor(kAutoclickRingCircleColor);
  canvas->DrawCircle(center, radius + kAutoclickRingArcWidth, flags);
  canvas->DrawCircle(center, radius - kAutoclickRingArcWidth, flags);

  // Draw the background of the arc.
  flags.setColor(kAutoclickRingUnderArcColor);
  canvas->DrawCircle(center, radius, flags);

  // Draw the arc.
  SkPath arc_path;
  arc_path.addArc(SkRect::MakeXYWH(center.x() - radius, center.y() - radius,
                                   2 * radius, 2 * radius),
                  kAutoclickRingAngleStartValue,
                  end_angle - kAutoclickRingAngleStartValue);
  flags.setStrokeWidth(kAutoclickRingArcWidth);
  flags.setColor(kAutoclickRingArcColor);

  canvas->DrawPath(arc_path, flags);
}

}  // namespace

// View of the AutoclickRingHandler. Draws the actual contents and updates as
// the animation proceeds. It also maintains the views::Widget that the
// animation is shown in.
class AutoclickRingHandler::AutoclickRingView : public views::View {
 public:
  AutoclickRingView(const gfx::Point& event_location,
                    views::Widget* ring_widget,
                    int radius)
      : views::View(),
        widget_(ring_widget),
        current_angle_(kAutoclickRingAngleStartValue),
        radius_(radius) {
    widget_->SetContentsView(this);

    // We are owned by the AutoclickRingHandler.
    set_owned_by_client();
    SetLocation(event_location);
  }

  ~AutoclickRingView() override = default;

  void SetLocation(const gfx::Point& new_event_location) {
    gfx::Point point = new_event_location;
    widget_->SetBounds(
        gfx::Rect(point.x() - (radius_ + kAutoclickRingArcWidth * 2),
                  point.y() - (radius_ + kAutoclickRingArcWidth * 2),
                  GetPreferredSize().width(), GetPreferredSize().height()));
    widget_->Show();
    widget_->GetNativeView()->layer()->SetOpacity(1.0);
  }

  void UpdateWithGrowAnimation(gfx::Animation* animation) {
    // Update the portion of the circle filled so far and re-draw.
    current_angle_ = animation->CurrentValueBetween(
        kAutoclickRingAngleStartValue, kAutoclickRingAngleEndValue);
    SchedulePaint();
  }

  void UpdateWithShrinkAnimation(gfx::Animation* animation) {
    current_angle_ = animation->CurrentValueBetween(
        kAutoclickRingAngleStartValue, kAutoclickRingAngleEndValue);
    SchedulePaint();
  }

  void SetSize(int radius) { radius_ = radius; }

 private:
  // Overridden from views::View.
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(2 * (radius_ + kAutoclickRingArcWidth * 2),
                     2 * (radius_ + kAutoclickRingArcWidth * 2));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Point center(GetPreferredSize().width() / 2,
                      GetPreferredSize().height() / 2);
    canvas->Save();

    PaintAutoclickRing(canvas, center, radius_, current_angle_);

    canvas->Restore();
  }

  views::Widget* widget_;
  int current_angle_;
  int radius_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickRingView);
};

////////////////////////////////////////////////////////////////////////////////

// AutoclickRingHandler, public
AutoclickRingHandler::AutoclickRingHandler()
    : gfx::LinearAnimation(nullptr),
      ring_widget_(nullptr),
      current_animation_type_(AnimationType::NONE),
      radius_(kAutoclickRingInnerRadius) {}

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

void AutoclickRingHandler::SetSize(int radius) {
  radius_ = radius;
  if (view_)
    view_->SetSize(radius);
}
////////////////////////////////////////////////////////////////////////////////

// AutoclickRingHandler, private
void AutoclickRingHandler::StartAnimation(base::TimeDelta delay) {
  switch (current_animation_type_) {
    case AnimationType::GROW_ANIMATION: {
      view_.reset(
          new AutoclickRingView(tap_down_location_, ring_widget_, radius_));
      SetDuration(delay);
      Start();
      break;
    }
    case AnimationType::SHRINK_ANIMATION: {
      view_.reset(
          new AutoclickRingView(tap_down_location_, ring_widget_, radius_));
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
      view_->SetLocation(tap_down_location_);
      view_->UpdateWithGrowAnimation(this);
      break;
    case AnimationType::SHRINK_ANIMATION:
      view_->SetLocation(tap_down_location_);
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
