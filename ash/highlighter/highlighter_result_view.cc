// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_result_view.h"

#include <memory>

#include "ash/highlighter/highlighter_gesture_util.h"
#include "ash/highlighter/highlighter_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Variables for rendering the highlight result. Sizes in DIP.
constexpr float kCornerCircleRadius = 6;
constexpr float kStrokeFillWidth = 2;
constexpr float kStrokeOutlineWidth = 1;
constexpr float kOutsetForAntialiasing = 1;
constexpr float kResultLayerMargin =
    kOutsetForAntialiasing + kCornerCircleRadius;

constexpr int kInnerFillOpacity = 0x0D;
const SkColor kInnerFillColor = SkColorSetRGB(0x00, 0x00, 0x00);

constexpr int kStrokeFillOpacity = 0xFF;
const SkColor kStrokeFillColor = SkColorSetRGB(0xFF, 0xFF, 0xFF);

constexpr int kStrokeOutlineOpacity = 0x14;
const SkColor kStrokeOutlineColor = SkColorSetRGB(0x00, 0x00, 0x00);

constexpr int kCornerCircleOpacity = 0xFF;
const SkColor kCornerCircleColorLT = SkColorSetRGB(0x42, 0x85, 0xF4);
const SkColor kCornerCircleColorRT = SkColorSetRGB(0xEA, 0x43, 0x35);
const SkColor kCornerCircleColorLB = SkColorSetRGB(0x34, 0xA8, 0x53);
const SkColor kCornerCircleColorRB = SkColorSetRGB(0xFB, 0xBC, 0x05);

constexpr int kResultFadeinDelayMs = 200;
constexpr int kResultFadeinDurationMs = 400;
constexpr int kResultFadeoutDelayMs = 500;
constexpr int kResultFadeoutDurationMs = 200;

constexpr int kResultInPlaceFadeinDelayMs = 100;
constexpr int kResultInPlaceFadeinDurationMs = 500;

constexpr float kInitialScale = 1.2;

class ResultLayer : public ui::Layer, public ui::LayerDelegate {
 public:
  ResultLayer(const gfx::Rect& bounds);

  ResultLayer(const ResultLayer&) = delete;
  ResultLayer& operator=(const ResultLayer&) = delete;

 private:
  // ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}
  void OnPaintLayer(const ui::PaintContext& context) override;

  void DrawVerticalBar(gfx::Canvas& canvas,
                       float x,
                       float y,
                       float height,
                       cc::PaintFlags& flags);
  void DrawHorizontalBar(gfx::Canvas& canvas,
                         float x,
                         float y,
                         float width,
                         cc::PaintFlags& flags);
};

ResultLayer::ResultLayer(const gfx::Rect& box) {
  SetName("HighlighterResultView:ResultLayer");
  gfx::Rect bounds = box;
  bounds.Inset(-kResultLayerMargin);
  SetBounds(bounds);
  SetFillsBoundsOpaquely(false);
  SetMasksToBounds(false);
  set_delegate(this);
}

void ResultLayer::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, size());
  gfx::Canvas& canvas = *recorder.canvas();

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  const float left = kResultLayerMargin;
  const float right = size().width() - kResultLayerMargin;
  const float width = right - left;

  const float top = kResultLayerMargin;
  const float bottom = size().height() - kResultLayerMargin;
  const float height = bottom - top;

  flags.setColor(SkColorSetA(kInnerFillColor, kInnerFillOpacity));
  canvas.DrawRect(gfx::RectF(left, top, width, height), flags);

  DrawVerticalBar(canvas, left, top, height, flags);
  DrawVerticalBar(canvas, right, top, height, flags);
  DrawHorizontalBar(canvas, left, top, width, flags);
  DrawHorizontalBar(canvas, left, bottom, width, flags);

  flags.setColor(SkColorSetA(kCornerCircleColorLT, kCornerCircleOpacity));
  canvas.DrawCircle(gfx::PointF(left, top), kCornerCircleRadius, flags);

  flags.setColor(SkColorSetA(kCornerCircleColorRT, kCornerCircleOpacity));
  canvas.DrawCircle(gfx::PointF(right, top), kCornerCircleRadius, flags);

  flags.setColor(SkColorSetA(kCornerCircleColorLB, kCornerCircleOpacity));
  canvas.DrawCircle(gfx::PointF(right, bottom), kCornerCircleRadius, flags);

  flags.setColor(SkColorSetA(kCornerCircleColorRB, kCornerCircleOpacity));
  canvas.DrawCircle(gfx::PointF(left, bottom), kCornerCircleRadius, flags);
}

void ResultLayer::DrawVerticalBar(gfx::Canvas& canvas,
                                  float x,
                                  float y,
                                  float height,
                                  cc::PaintFlags& flags) {
  const float x_fill = x - kStrokeFillWidth / 2;
  const float x_outline_left = x_fill - kStrokeOutlineWidth;
  const float x_outline_right = x_fill + kStrokeFillWidth;

  flags.setColor(SkColorSetA(kStrokeFillColor, kStrokeFillOpacity));
  canvas.DrawRect(gfx::RectF(x_fill, y, kStrokeFillWidth, height), flags);

  flags.setColor(SkColorSetA(kStrokeOutlineColor, kStrokeOutlineOpacity));
  canvas.DrawRect(gfx::RectF(x_outline_left, y, kStrokeOutlineWidth, height),
                  flags);
  canvas.DrawRect(gfx::RectF(x_outline_right, y, kStrokeOutlineWidth, height),
                  flags);
}

void ResultLayer::DrawHorizontalBar(gfx::Canvas& canvas,
                                    float x,
                                    float y,
                                    float width,
                                    cc::PaintFlags& flags) {
  const float y_fill = y - kStrokeFillWidth / 2;
  const float y_outline_left = y_fill - kStrokeOutlineWidth;
  const float y_outline_right = y_fill + kStrokeFillWidth;

  flags.setColor(SkColorSetA(kStrokeFillColor, kStrokeFillOpacity));
  canvas.DrawRect(gfx::RectF(x, y_fill, width, kStrokeFillWidth), flags);

  flags.setColor(SkColorSetA(kStrokeOutlineColor, kStrokeOutlineOpacity));
  canvas.DrawRect(gfx::RectF(x, y_outline_left, width, kStrokeOutlineWidth),
                  flags);
  canvas.DrawRect(gfx::RectF(x, y_outline_right, width, kStrokeOutlineWidth),
                  flags);
}

}  // namespace

HighlighterResultView::HighlighterResultView() = default;

HighlighterResultView::~HighlighterResultView() = default;

// static
views::UniqueWidgetPtr HighlighterResultView::Create(
    aura::Window* root_window) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = "HighlighterResult";
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);
  params.layer_type = ui::LAYER_SOLID_COLOR;

  auto widget = views::UniqueWidgetPtr(
      std::make_unique<views::Widget>(std::move(params)));
  widget->SetContentsView(std::make_unique<HighlighterResultView>());
  widget->SetFullscreen(true);
  widget->Show();
  return widget;
}

void HighlighterResultView::Animate(const gfx::RectF& bounds,
                                    HighlighterGestureType gesture_type,
                                    base::OnceClosure done) {
  ui::Layer* layer = GetWidget()->GetLayer();

  base::TimeDelta delay;
  base::TimeDelta duration;

  if (gesture_type == HighlighterGestureType::kHorizontalStroke) {
    // The original stroke is fading out in place.
    // Fade in a solid transparent rectangle.
    result_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
    result_layer_->SetName("HighlighterResultView:SOLID_LAYER");
    result_layer_->SetBounds(gfx::ToEnclosingRect(bounds));
    result_layer_->SetFillsBoundsOpaquely(false);
    result_layer_->SetMasksToBounds(false);
    result_layer_->SetColor(fast_ink::FastInkPoints::kDefaultColor);

    layer->Add(result_layer_.get());

    delay = base::Milliseconds(kResultInPlaceFadeinDelayMs);
    duration = base::Milliseconds(kResultInPlaceFadeinDurationMs);
  } else {
    DCHECK(gesture_type == HighlighterGestureType::kClosedShape);
    // The original stroke is fading out and inflating.
    // Fade in the deflating lens overlay.
    result_layer_ = std::make_unique<ResultLayer>(gfx::ToEnclosingRect(bounds));
    layer->Add(result_layer_.get());

    gfx::Transform transform;
    const gfx::PointF pivot = bounds.CenterPoint();
    transform.Translate(pivot.x() * (1 - kInitialScale),
                        pivot.y() * (1 - kInitialScale));
    transform.Scale(kInitialScale, kInitialScale);
    layer->SetTransform(transform);

    delay = base::Milliseconds(kResultFadeinDelayMs);
    duration = base::Milliseconds(kResultFadeinDurationMs);
  }

  layer->SetOpacity(0);

  animation_timer_ = std::make_unique<base::OneShotTimer>();
  animation_timer_->Start(
      FROM_HERE, delay,
      base::BindOnce(&HighlighterResultView::FadeIn, base::Unretained(this),
                     duration, std::move(done)));
}

void HighlighterResultView::FadeIn(const base::TimeDelta& duration,
                                   base::OnceClosure done) {
  ui::Layer* layer = GetWidget()->GetLayer();

  {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(duration);
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    layer->SetOpacity(1);

    gfx::Transform transform;
    transform.Scale(1, 1);
    transform.Translate(0, 0);
    layer->SetTransform(transform);
  }

  animation_timer_ = std::make_unique<base::OneShotTimer>();
  animation_timer_->Start(
      FROM_HERE, duration + base::Milliseconds(kResultFadeoutDelayMs),
      base::BindOnce(&HighlighterResultView::FadeOut, base::Unretained(this),
                     std::move(done)));
}

void HighlighterResultView::FadeOut(base::OnceClosure done) {
  ui::Layer* layer = GetWidget()->GetLayer();

  base::TimeDelta duration = base::Milliseconds(kResultFadeoutDurationMs);

  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(duration);
  settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
  layer->SetOpacity(0);

  animation_timer_ = std::make_unique<base::OneShotTimer>();
  animation_timer_->Start(FROM_HERE, duration, std::move(done));
}

}  // namespace ash
