// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_hud_renderer.h"

#include "base/time/time.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

constexpr int kPointRadius = 20;
constexpr SkColor kProjectionFillColor = SkColorSetRGB(0xF5, 0xF5, 0xDC);
constexpr SkColor kProjectionStrokeColor = SK_ColorGRAY;
constexpr int kProjectionAlpha = 0xB0;
constexpr base::TimeDelta kFadeoutDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr int kFadeoutFrameRate = 60;

// TouchPointView draws a single touch point.
class TouchPointView : public views::View,
                       public views::AnimationDelegateViews,
                       public views::WidgetObserver {
 public:
  explicit TouchPointView(views::Widget* parent_widget)
      : views::AnimationDelegateViews(this) {
    set_owned_by_client();
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    SetSize(gfx::Size(2 * kPointRadius + 2, 2 * kPointRadius + 2));

    parent_widget->GetContentsView()->AddChildView(this);
    widget_observer_.Add(parent_widget);
  }

  ~TouchPointView() override = default;

  // Begins fadeout animation. After this is called, |this| owns itself and is
  // responsible for deleting itself when the animation ends or the host widget
  // is destroyed.
  void FadeOut(std::unique_ptr<TouchPointView> self) {
    DCHECK_EQ(this, self.get());
    owned_self_reference_ = std::move(self);
    fadeout_.reset(
        new gfx::LinearAnimation(kFadeoutDuration, kFadeoutFrameRate, this));
    fadeout_->Start();
  }

  void UpdateLocation(const ui::LocatedEvent& touch) {
    SetPosition(
        gfx::Point(parent()->GetMirroredXInView(touch.root_location().x()) -
                       kPointRadius - 1,
                   touch.root_location().y() - kPointRadius - 1));
  }

 private:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    int alpha = kProjectionAlpha;
    if (fadeout_)
      alpha = static_cast<int>(fadeout_->CurrentValueBetween(alpha, 0));

    cc::PaintFlags fill_flags;
    fill_flags.setAlpha(alpha);

    constexpr SkColor gradient_colors[2] = {kProjectionFillColor,
                                            kProjectionStrokeColor};
    constexpr SkScalar gradient_pos[2] = {SkFloatToScalar(0.9f),
                                          SkFloatToScalar(1.0f)};
    constexpr gfx::Point center(kPointRadius + 1, kPointRadius + 1);

    fill_flags.setShader(cc::PaintShader::MakeRadialGradient(
        gfx::PointToSkPoint(center), SkIntToScalar(kPointRadius),
        gradient_colors, gradient_pos, base::size(gradient_colors),
        SkTileMode::kMirror));
    canvas->DrawCircle(center, SkIntToScalar(kPointRadius), fill_flags);

    cc::PaintFlags stroke_flags;
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setColor(kProjectionStrokeColor);
    stroke_flags.setAlpha(alpha);
    canvas->DrawCircle(center, SkIntToScalar(kPointRadius), stroke_flags);
  }

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override {
    DCHECK_EQ(fadeout_.get(), animation);
    owned_self_reference_.reset();
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    DCHECK_EQ(fadeout_.get(), animation);
    SchedulePaint();
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    owned_self_reference_.reset();
  }

  std::unique_ptr<gfx::Animation> fadeout_;

  // When non-null, |owned_self_reference_| refers to |this|, and |this| owns
  // itself. This should be non-null when fading out, and null otherwise.
  std::unique_ptr<TouchPointView> owned_self_reference_;

  ScopedObserver<views::Widget, views::WidgetObserver> widget_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(TouchPointView);
};

TouchHudRenderer::TouchHudRenderer(views::Widget* parent_widget)
    : parent_widget_(parent_widget) {
  parent_widget_->AddObserver(this);
}

TouchHudRenderer::~TouchHudRenderer() {
  if (parent_widget_)
    parent_widget_->RemoveObserver(this);
}

void TouchHudRenderer::Clear() {
  points_.clear();
}

void TouchHudRenderer::HandleTouchEvent(const ui::TouchEvent& event) {
  int id = event.pointer_details().id;
  auto iter = points_.find(id);
  if (event.type() == ui::ET_TOUCH_PRESSED) {
    if (iter != points_.end())
      points_.erase(iter);

    auto point = std::make_unique<TouchPointView>(parent_widget_);
    point->UpdateLocation(event);
    auto result = points_.insert(std::make_pair(id, std::move(point)));
    DCHECK(result.second);
    return;
  }

  if (iter == points_.end())
    return;

  if (event.type() == ui::ET_TOUCH_RELEASED ||
      event.type() == ui::ET_TOUCH_CANCELLED) {
    TouchPointView* view = iter->second.get();
    view->FadeOut(std::move(iter->second));
    points_.erase(iter);
  } else {
    iter->second->UpdateLocation(event);
  }
}

void TouchHudRenderer::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, parent_widget_);
  parent_widget_->RemoveObserver(this);
  parent_widget_ = nullptr;
  Clear();
}

}  // namespace ash
