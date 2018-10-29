// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/tap_visualizer/tap_renderer.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace tap_visualizer {

const int kPointRadius = 20;
const SkColor kProjectionFillColor = SkColorSetRGB(0xF5, 0xF5, 0xDC);
const SkColor kProjectionStrokeColor = SK_ColorGRAY;
const int kProjectionAlpha = 0xB0;
constexpr base::TimeDelta kFadeoutDuration =
    base::TimeDelta::FromMilliseconds(250);
const int kFadeoutFrameRate = 60;

// TouchPointView draws a single touch point. Owned by the views hierarchy.
// Deletes itself upon fade-out completion.
class TouchPointView : public views::View, public gfx::AnimationDelegate {
 public:
  TouchPointView()
      : circle_center_(kPointRadius + 1, kPointRadius + 1),
        gradient_center_(SkPoint::Make(kPointRadius + 1, kPointRadius + 1)) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    SetSize(gfx::Size(2 * kPointRadius + 2, 2 * kPointRadius + 2));

    stroke_flags_.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags_.setColor(kProjectionStrokeColor);

    gradient_colors_[0] = kProjectionFillColor;
    gradient_colors_[1] = kProjectionStrokeColor;

    gradient_pos_[0] = SkFloatToScalar(0.9f);
    gradient_pos_[1] = SkFloatToScalar(1.0f);
  }

  void UpdateTouch(const ui::TouchEvent& touch) {
    if (touch.type() == ui::ET_TOUCH_RELEASED ||
        touch.type() == ui::ET_TOUCH_CANCELLED) {
      fadeout_.reset(
          new gfx::LinearAnimation(kFadeoutDuration, kFadeoutFrameRate, this));
      fadeout_->Start();
    } else {
      gfx::Point location = touch.root_location();
      ConvertPointFromScreen(parent(), &location);
      SetX(parent()->GetMirroredXInView(location.x()) - kPointRadius - 1);
      SetY(location.y() - kPointRadius - 1);
    }
  }

  // Destroys the view, which removes it from its parent.
  void Destroy() { delete this; }

 private:
  ~TouchPointView() override = default;

  // Overridden from views::View.
  void OnPaint(gfx::Canvas* canvas) override {
    int alpha = kProjectionAlpha;
    if (fadeout_)
      alpha = static_cast<int>(fadeout_->CurrentValueBetween(alpha, 0));
    fill_flags_.setAlpha(alpha);
    stroke_flags_.setAlpha(alpha);
    fill_flags_.setShader(cc::PaintShader::MakeRadialGradient(
        gradient_center_, SkIntToScalar(kPointRadius), gradient_colors_,
        gradient_pos_, arraysize(gradient_colors_),
        SkShader::kMirror_TileMode));
    canvas->DrawCircle(circle_center_, SkIntToScalar(kPointRadius),
                       fill_flags_);
    canvas->DrawCircle(circle_center_, SkIntToScalar(kPointRadius),
                       stroke_flags_);
  }

  // Overridden from gfx::AnimationDelegate.
  void AnimationEnded(const gfx::Animation* animation) override {
    DCHECK_EQ(fadeout_.get(), animation);
    delete this;
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    DCHECK_EQ(fadeout_.get(), animation);
    SchedulePaint();
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  const gfx::Point circle_center_;
  const SkPoint gradient_center_;

  cc::PaintFlags fill_flags_;
  cc::PaintFlags stroke_flags_;
  SkColor gradient_colors_[2];
  SkScalar gradient_pos_[2];

  std::unique_ptr<gfx::Animation> fadeout_;

  DISALLOW_COPY_AND_ASSIGN(TouchPointView);
};

TapRenderer::TapRenderer(std::unique_ptr<views::Widget> widget)
    : widget_(std::move(widget)) {
  DCHECK(widget_);
}

TapRenderer::~TapRenderer() = default;

void TapRenderer::HandleTouchEvent(const ui::TouchEvent& event) {
  const int id = event.pointer_details().id;
  if (event.type() == ui::ET_TOUCH_PRESSED) {
    TouchPointView* point = new TouchPointView();
    widget_->GetContentsView()->AddChildView(point);
    point->UpdateTouch(event);
    std::pair<std::map<int, TouchPointView*>::iterator, bool> result =
        points_.insert(std::make_pair(id, point));
    // If a |TouchPointView| is already mapped to the touch id, destroy it and
    // replace it with the new one.
    if (!result.second) {
      result.first->second->Destroy();
      result.first->second = point;
    }
  } else {
    std::map<int, TouchPointView*>::iterator iter = points_.find(id);
    if (iter != points_.end()) {
      iter->second->UpdateTouch(event);
      if (event.type() == ui::ET_TOUCH_RELEASED ||
          event.type() == ui::ET_TOUCH_CANCELLED) {
        points_.erase(iter);
        // View will fade out then delete itself.
      }
    }
  }
}

}  // namespace tap_visualizer
