// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_hud_renderer.h"

#include <memory>

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

constexpr int kPointRadius = 20;
constexpr SkColor4f kProjectionFillColor{0.96f, 0.96f, 0.86f, 1.0f};
constexpr SkColor4f kProjectionStrokeColor = SkColors::kGray;
constexpr float kProjectionAlpha = 0xB0 / 255.0f;
constexpr base::TimeDelta kFadeoutDuration = base::Milliseconds(250);
constexpr int kFadeoutFrameRate = 60;

// TouchPointView draws a single touch point.
class TouchPointView : public views::View,
                       public views::AnimationDelegateViews,
                       public views::WidgetObserver {
  METADATA_HEADER(TouchPointView, views::View)

 public:
  explicit TouchPointView(views::Widget* parent_widget)
      : views::AnimationDelegateViews(this) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    SetSize(gfx::Size(2 * kPointRadius + 2, 2 * kPointRadius + 2));

    widget_observation_.Observe(parent_widget);
  }

  TouchPointView(const TouchPointView&) = delete;
  TouchPointView& operator=(const TouchPointView&) = delete;

  ~TouchPointView() override = default;

  // Begins fadeout animation. After this is called, |this| owns itself and is
  // responsible for deleting itself when the animation ends or the host widget
  // is destroyed.
  void FadeOut(std::unique_ptr<TouchPointView> self) {
    DCHECK_EQ(this, self.get());
    owned_self_reference_ = std::move(self);
    fadeout_ = std::make_unique<gfx::LinearAnimation>(kFadeoutDuration,
                                                      kFadeoutFrameRate, this);
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
    const float alpha =
        fadeout_ ? fadeout_->CurrentValueBetween(kProjectionAlpha, 0.0f)
                 : kProjectionAlpha;

    cc::PaintFlags fill_flags;
    fill_flags.setAlphaf(alpha);

    constexpr SkColor4f gradient_colors[2] = {kProjectionFillColor,
                                              kProjectionStrokeColor};
    constexpr SkScalar gradient_pos[2] = {SkFloatToScalar(0.9f),
                                          SkFloatToScalar(1.0f)};
    constexpr gfx::Point center(kPointRadius + 1, kPointRadius + 1);

    fill_flags.setShader(cc::PaintShader::MakeRadialGradient(
        gfx::PointToSkPoint(center), SkIntToScalar(kPointRadius),
        gradient_colors, gradient_pos, std::size(gradient_colors),
        SkTileMode::kMirror));
    canvas->DrawCircle(center, SkIntToScalar(kPointRadius), fill_flags);

    cc::PaintFlags stroke_flags;
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setColor(kProjectionStrokeColor);
    stroke_flags.setAlphaf(alpha);
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

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

BEGIN_METADATA(TouchPointView)
END_METADATA

TouchHudRenderer::TouchHudRenderer(views::Widget* parent_widget)
    : parent_widget_(parent_widget) {
  parent_widget_->AddObserver(this);
}

TouchHudRenderer::~TouchHudRenderer() {
  if (parent_widget_)
    parent_widget_->RemoveObserver(this);
  CHECK(!IsInObserverList());
}

void TouchHudRenderer::Clear() {
  points_.clear();
}

void TouchHudRenderer::HandleTouchEvent(const ui::TouchEvent& event) {
  int id = event.pointer_details().id;
  auto iter = points_.find(id);
  if (event.type() == ui::EventType::kTouchPressed) {
    if (iter != points_.end()) {
      TouchPointView* view = iter->second;
      view->parent()->RemoveChildViewT(view);
      points_.erase(iter);
    }

    TouchPointView* point = parent_widget_->GetContentsView()->AddChildView(
        std::make_unique<TouchPointView>(parent_widget_));
    point->UpdateLocation(event);
    auto result = points_.insert(std::make_pair(id, point));
    DCHECK(result.second);
    return;
  }

  if (iter == points_.end())
    return;

  if (event.type() == ui::EventType::kTouchReleased ||
      event.type() == ui::EventType::kTouchCancelled) {
    TouchPointView* view = iter->second;
    view->FadeOut(view->parent()->RemoveChildViewT(view));
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
