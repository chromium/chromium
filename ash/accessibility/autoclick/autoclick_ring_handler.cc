// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/autoclick/autoclick_ring_handler.h"

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/view.h"

namespace ash {
namespace {

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
  AutoclickRingView(views::Widget* ring_widget, int radius)
      : views::View(), widget_(ring_widget), radius_(radius) {}

  AutoclickRingView(const AutoclickRingView&) = delete;
  AutoclickRingView& operator=(const AutoclickRingView&) = delete;

  ~AutoclickRingView() override = default;

  static AutoclickRingView* Create(const gfx::Point& event_location,
                                   views::Widget* ring_widget,
                                   int radius) {
    AutoclickRingView* ring_view = ring_widget->SetContentsView(
        std::make_unique<AutoclickRingView>(ring_widget, radius));
    ring_view->SetLocation(event_location);
    return ring_view;
  }

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

  void SetSize(int radius) { radius_ = radius; }

 private:
  // Overridden from views::View.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
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

  raw_ptr<views::Widget> widget_;
  int radius_;
  int current_angle_ = kAutoclickRingAngleStartValue;
};

////////////////////////////////////////////////////////////////////////////////

// AutoclickRingHandler, public
AutoclickRingHandler::AutoclickRingHandler() : gfx::LinearAnimation(nullptr) {}

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
  current_animation_type_ = AnimationType::kGrowAnimation;
  animation_duration_ = duration;
  StartAnimation(animation_duration_);
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
    case AnimationType::kGrowAnimation: {
      DCHECK(!view_);
      view_ =
          AutoclickRingView::Create(tap_down_location_, ring_widget_, radius_);
      SetDuration(delay);
      Start();
      break;
    }
    case AnimationType::kNone:
      NOTREACHED();
  }
}

void AutoclickRingHandler::StopAutoclickRing() {
  // Since, Animation::Stop() calls AnimationStopped(), we need to reset the
  // |current_animation_type_| before Stop(), otherwise AnimationStopped() may
  // start the timer again.
  current_animation_type_ = AnimationType::kNone;
  Stop();
  if (view_) {
    ring_widget_->GetRootView()->RemoveChildViewT(view_.get());
    view_ = nullptr;
  }
}

void AutoclickRingHandler::AnimateToState(double state) {
  DCHECK(view_);
  switch (current_animation_type_) {
    case AnimationType::kGrowAnimation:
      view_->SetLocation(tap_down_location_);
      view_->UpdateWithGrowAnimation(this);
      break;
    case AnimationType::kNone:
      NOTREACHED();
  }
}

void AutoclickRingHandler::AnimationStopped() {
  switch (current_animation_type_) {
    case AnimationType::kGrowAnimation:
      current_animation_type_ = AnimationType::kNone;
      break;
    case AnimationType::kNone:
      // Fall through to reset the view.
      if (view_) {
        ring_widget_->GetRootView()->RemoveChildViewT(view_.get());
        view_ = nullptr;
      }
      break;
  }
}

}  // namespace ash
