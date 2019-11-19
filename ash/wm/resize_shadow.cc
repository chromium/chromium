// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/resize_shadow.h"

#include <memory>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"

namespace {

// The width of the resize shadow that appears on the hovered edge of the
// window.
constexpr int kVisualThickness = 8;

// The corner radius of the resize shadow, which not coincidentally matches
// the corner radius of the actual window.
static constexpr int kCornerRadiusOfResizeShadow = 2;
static constexpr int kCornerRadiusOfWindow = 2;

// This class simply draws a roundrect. The layout and tiling is handled by
// ResizeShadow and NinePatchLayer.
class ResizeShadowImageSource : public gfx::CanvasImageSource {
 public:
  ResizeShadowImageSource()
      : gfx::CanvasImageSource(gfx::Size(kImageSide, kImageSide)) {}

  ~ResizeShadowImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorBLACK);
    canvas->DrawRoundRect(gfx::RectF(gfx::SizeF(size())),
                          kCornerRadiusOfResizeShadow, paint);
  }

 private:
  // The image has to have enough space to depict the visual thickness (left and
  // right) plus an inset for extending beneath the window's rounded corner plus
  // one pixel for the center of the nine patch.
  static constexpr int kImageSide =
      2 * (kVisualThickness + kCornerRadiusOfWindow) + 1;

  DISALLOW_COPY_AND_ASSIGN(ResizeShadowImageSource);
};

}  // namespace

namespace ash {

ResizeShadow::ResizeShadow(aura::Window* window)
    : window_(window), last_hit_test_(HTNOWHERE) {
  window_->AddObserver(this);

  // Use a NinePatchLayer to tile the shadow image (which is simply a
  // roundrect).
  layer_.reset(new ui::Layer(ui::LAYER_NINE_PATCH));
  layer_->set_name("WindowResizeShadow");
  layer_->SetFillsBoundsOpaquely(false);
  layer_->SetOpacity(0.f);
  layer_->SetVisible(false);

  static base::NoDestructor<gfx::ImageSkia> shadow_image;

  if (shadow_image->isNull()) {
    auto* source = new ResizeShadowImageSource();
    *shadow_image = gfx::ImageSkia(base::WrapUnique(source), source->size());
  }
  layer_->UpdateNinePatchLayerImage(*shadow_image);
  gfx::Rect aperture(shadow_image->size());
  constexpr gfx::Insets kApertureInsets(kVisualThickness +
                                        kCornerRadiusOfWindow);
  aperture.Inset(kApertureInsets);
  layer_->UpdateNinePatchLayerAperture(aperture);
  layer_->UpdateNinePatchLayerBorder(
      gfx::Rect(kApertureInsets.left(), kApertureInsets.top(),
                kApertureInsets.width(), kApertureInsets.height()));

  ReparentLayer();
}

ResizeShadow::~ResizeShadow() {
  window_->RemoveObserver(this);
}

void ResizeShadow::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds,
                                         ui::PropertyChangeReason reason) {
  UpdateBoundsAndVisibility();
}

void ResizeShadow::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  ReparentLayer();
}

void ResizeShadow::OnWindowStackingChanged(aura::Window* window) {
  ReparentLayer();
}

void ResizeShadow::ShowForHitTest(int hit) {
  // Don't start animations unless something changed.
  if (hit == last_hit_test_)
    return;
  last_hit_test_ = hit;

  UpdateBoundsAndVisibility();
}

void ResizeShadow::Hide() {
  ShowForHitTest(HTNOWHERE);
}

void ResizeShadow::ReparentLayer() {
  DCHECK(window_->layer()->parent());
  if (layer_->parent() != window_->layer()->parent())
    window_->layer()->parent()->Add(layer_.get());
  layer_->parent()->StackBelow(layer_.get(), window_->layer());
}

void ResizeShadow::UpdateBoundsAndVisibility() {
  // The shadow layer is positioned such that one or two edges will stick out
  // from underneath |window_|. Thus |window_| occludes the rest of the
  // roundrect.
  const int hit = last_hit_test_;
  bool show_top = hit == HTTOPLEFT || hit == HTTOP || hit == HTTOPRIGHT;
  bool show_left = hit == HTTOPLEFT || hit == HTLEFT || hit == HTBOTTOMLEFT;
  bool show_bottom =
      hit == HTBOTTOMLEFT || hit == HTBOTTOM || hit == HTBOTTOMRIGHT;
  bool show_right = hit == HTTOPRIGHT || hit == HTRIGHT || hit == HTBOTTOMRIGHT;

  const int outset = -kVisualThickness;
  gfx::Insets outsets(show_top ? outset : 0, show_left ? outset : 0,
                      show_bottom ? outset : 0, show_right ? outset : 0);
  bool visible = !outsets.IsEmpty();
  if (!visible && !layer_->GetTargetVisibility())
    return;

  if (visible) {
    gfx::Rect bounds = window_->bounds();
    bounds.Inset(outsets);
    layer_->SetBounds(bounds);
  }

  // The resize shadow snaps in but fades out.
  ui::ScopedLayerAnimationSettings settings(layer_->GetAnimator());
  if (!visible) {
    constexpr int kShadowFadeOutDurationMs = 100;
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kShadowFadeOutDurationMs));
  }
  constexpr float kShadowTargetOpacity = 0.5f;
  layer_->SetOpacity(visible ? kShadowTargetOpacity : 0.f);
  layer_->SetVisible(visible);
}

}  // namespace ash
