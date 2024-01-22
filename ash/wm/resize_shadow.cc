// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/resize_shadow.h"

#include <map>

#include "ash/root_window_controller.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_source.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"

namespace {

// The features that uniquely described a shadow's appearance.
struct ShadowFeaturesKey {
  bool operator==(const ShadowFeaturesKey& other) const {
    return MakeTuple() == other.MakeTuple();
  }
  bool operator<(const ShadowFeaturesKey& other) const {
    return MakeTuple() < other.MakeTuple();
  }

  std::tuple<int, int, SkColor> MakeTuple() const {
    return std::make_tuple(width, corner_radius, color);
  }

  // The total width of the shadow region.
  int width = 0;
  int corner_radius = 0;
  SkColor color = gfx::kPlaceholderColor;
};

// Get shadow image size with given init params.
int ShadowImageSize(const ash::ResizeShadow::InitParams& params) {
  //  The image has to have enough space to depict the visual thickness
  //  (left and right) plus an inset for extending beneath the window's
  //  rounded corner plus one pixel for the center of the nine patch.
  return 2 * (params.thickness + params.window_corner_radius) + 1;
}

// This class simply draws a roundrect. The layout and tiling is handled by
// ResizeShadow and NinePatchLayer.
class ResizeShadowImageSource : public gfx::CanvasImageSource {
 public:
  explicit ResizeShadowImageSource(const ShadowFeaturesKey& features)
      : gfx::CanvasImageSource(gfx::Size(features.width, features.width)),
        shadow_corner_radius_(features.corner_radius),
        color_(features.color) {}
  ResizeShadowImageSource(const ResizeShadowImageSource&) = delete;
  ResizeShadowImageSource& operator=(const ResizeShadowImageSource&) = delete;
  ~ResizeShadowImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags paint;
    paint.setAntiAlias(true);
    paint.setColor(color_);
    canvas->DrawRoundRect(gfx::RectF(gfx::SizeF(size())), shadow_corner_radius_,
                          paint);
  }

  int shadow_corner_radius_;
  SkColor color_;
};

// Calculate outsets of the |window_| based on a |hit_test| code and |thickness|
const gfx::Insets CalculateOutsets(int hit, int thickness) {
  bool show_top = hit == HTTOPLEFT || hit == HTTOP || hit == HTTOPRIGHT;
  bool show_left = hit == HTTOPLEFT || hit == HTLEFT || hit == HTBOTTOMLEFT;
  bool show_bottom =
      hit == HTBOTTOMLEFT || hit == HTBOTTOM || hit == HTBOTTOMRIGHT;
  bool show_right = hit == HTTOPRIGHT || hit == HTRIGHT || hit == HTBOTTOMRIGHT;

  const int outset = -thickness;
  return gfx::Insets::TLBR(show_top ? outset : 0, show_left ? outset : 0,
                           show_bottom ? outset : 0, show_right ? outset : 0);
}

// static
const gfx::ImageSkia& MakeShadowImageOnce(
    const ash::ResizeShadow::InitParams& params,
    const ui::ColorProvider* color_provider) {
  // Resolve the color with color type. If given a color ID, use color provider
  // to get the color value.
  SkColor color;
  if (absl::holds_alternative<SkColor>(params.color)) {
    color = absl::get<SkColor>(params.color);
  } else {
    CHECK(!!color_provider);
    color = color_provider->GetColor(absl::get<ui::ColorId>(params.color));
  }

  // Generate the shadow features key.
  const ShadowFeaturesKey features_key{ShadowImageSize(params),
                                       params.shadow_corner_radius, color};

  // Create a cache saving the shadow textures for each shadow features key.
  static base::NoDestructor<std::map<ShadowFeaturesKey, gfx::ImageSkia>>
      shadow_image_cache;
  auto iter = shadow_image_cache->find(features_key);

  // If requiring a new shadow texture, evict unused textures and create a new
  // one with given shadow features.
  if (iter == shadow_image_cache->end()) {
    std::erase_if(*shadow_image_cache, [](auto& key_and_image_source) {
      return key_and_image_source.second.IsUniquelyOwned();
    });

    iter =
        shadow_image_cache
            ->emplace(
                features_key,
                gfx::CanvasImageSource::MakeImageSkia<ResizeShadowImageSource>(
                    features_key))
            .first;
  }

  return iter->second;
}

}  // namespace

namespace ash {

ResizeShadow::ResizeShadow(aura::Window* window,
                           const InitParams& params,
                           ResizeShadowType type)
    : window_(window), params_(params), type_(type) {
  // Use a NinePatchLayer to tile the shadow image (which is simply a
  // roundrect).
  layer_ = std::make_unique<ui::Layer>(ui::LAYER_NINE_PATCH);
  layer_->SetName("WindowResizeShadow");
  layer_->SetFillsBoundsOpaquely(false);
  layer_->SetOpacity(0.f);
  layer_->SetVisible(false);
  // If use static color, create the shadow image. Otherwise, observe the color
  // provider source to update the shadow color.
  if (absl::holds_alternative<SkColor>(params.color)) {
    UpdateShadowLayer();
  } else {
    Observe(RootWindowController::ForWindow(window)->color_provider_source());
  }

  const gfx::Insets aperture_insets(params_.thickness +
                                    params_.window_corner_radius);
  const int image_size = ShadowImageSize(params_);
  gfx::Rect aperture(gfx::Size(image_size, image_size));
  aperture.Inset(aperture_insets);
  layer_->UpdateNinePatchLayerAperture(aperture);
  layer_->UpdateNinePatchLayerBorder(
      gfx::Rect(aperture_insets.left(), aperture_insets.top(),
                aperture_insets.width(), aperture_insets.height()));

  ReparentLayer();
}

ResizeShadow::~ResizeShadow() = default;

void ResizeShadow::OnColorProviderChanged() {
  // This function will also be called when the color provider source is
  // destroyed. We should guarantee the color provider exists.
  if (absl::holds_alternative<ui::ColorId>(params_.color) &&
      GetColorProviderSource()) {
    UpdateShadowLayer();
  }
}

void ResizeShadow::OnWindowParentToRootWindow() {
  if (absl::holds_alternative<ui::ColorId>(params_.color)) {
    Observe(RootWindowController::ForWindow(window_)->color_provider_source());
  }
}

void ResizeShadow::UpdateShadowLayer() {
  auto* color_provider_source = GetColorProviderSource();
  const auto& shadow_image = MakeShadowImageOnce(
      params_, color_provider_source ? color_provider_source->GetColorProvider()
                                     : nullptr);
  layer_->UpdateNinePatchLayerImage(shadow_image);
}

void ResizeShadow::ShowForHitTest(int hit) {
  UpdateHitTest(hit);
  visible_ = true;
  UpdateBoundsAndVisibility();
}

void ResizeShadow::Hide() {
  UpdateHitTest(HTNOWHERE);
  visible_ = false;
  UpdateBoundsAndVisibility();
}

void ResizeShadow::UpdateHitTest(int hit) {
  // Don't start animations unless something changed.
  if (hit == last_hit_test_)
    return;
  last_hit_test_ = hit;
}

void ResizeShadow::UpdateBoundsAndVisibility() {
  UpdateBounds(window_->bounds());
}

void ResizeShadow::UpdateBounds(const gfx::Rect& window_bounds) {
  // The shadow layer is positioned such that one or two edges will stick out
  // from underneath |window_|. Thus |window_| occludes the rest of the
  // roundrect.
  const gfx::Insets outsets =
      params_.hit_test_enabled
          ? CalculateOutsets(last_hit_test_, params_.thickness)
          : gfx::Insets(-params_.thickness);

  if (outsets.IsEmpty() && !layer_->GetTargetVisibility())
    return;

  visible_ &= !outsets.IsEmpty();
  if (visible_) {
    gfx::Rect bounds = window_bounds;
    bounds.Inset(outsets);
    layer_->SetBounds(bounds);
  }

  ui::ScopedLayerAnimationSettings settings(layer_->GetAnimator());
  if (!visible_)
    settings.SetTransitionDuration(
        base::Milliseconds(params_.hide_duration_ms));
  layer_->SetOpacity(visible_ ? params_.opacity : 0.f);
  layer_->SetVisible(visible_);
}

void ResizeShadow::ReparentLayer() {
  // This shadow could belong to a window that has been removed from its parent.
  // In that case, we should not try to access `window_`'s parent.
  if (!window_->layer()->parent())
    return;

  if (layer_->parent() != window_->layer()->parent())
    window_->layer()->parent()->Add(layer_.get());
  layer_->parent()->StackBelow(layer_.get(), window_->layer());
}

}  // namespace ash
