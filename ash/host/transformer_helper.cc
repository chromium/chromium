// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/transformer_helper.h"

#include <utility>

#include "ash/host/ash_window_tree_host.h"
#include "ash/host/root_window_transformer.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace ash {
namespace {

// A simple RootWindowTransformer without host insets.
class SimpleRootWindowTransformer : public RootWindowTransformer {
 public:
  SimpleRootWindowTransformer(const aura::Window* root_window,
                              const gfx::Transform& transform)
      : root_window_(root_window), transform_(transform) {}

  SimpleRootWindowTransformer(const SimpleRootWindowTransformer&) = delete;
  SimpleRootWindowTransformer& operator=(const SimpleRootWindowTransformer&) =
      delete;

  // RootWindowTransformer overrides:
  gfx::Transform GetTransform() const override { return transform_; }

  gfx::Transform GetInverseTransform() const override {
    gfx::Transform invert;
    if (!transform_.GetInverse(&invert))
      return transform_;
    return invert;
  }

  gfx::Rect GetRootWindowBounds(const gfx::Size& host_size) const override {
    gfx::Rect host_bounds_in_pixels(host_size);
    gfx::RectF host_bounds_in_dips = gfx::ConvertRectToDips(
        host_bounds_in_pixels, root_window_->layer()->device_scale_factor());
    gfx::RectF root_window_bounds =
        GetInverseTransform().MapRect(host_bounds_in_dips);
    return gfx::Rect(gfx::ToFlooredSize(root_window_bounds.size()));
  }

  gfx::Insets GetHostInsets() const override { return gfx::Insets(); }
  gfx::Transform GetInsetsAndScaleTransform() const override {
    return transform_;
  }

 private:
  ~SimpleRootWindowTransformer() override = default;

  raw_ptr<const aura::Window> root_window_;
  const gfx::Transform transform_;
};

}  // namespace

TransformerHelper::TransformerHelper(AshWindowTreeHost* ash_host)
    : ash_host_(ash_host) {}

TransformerHelper::~TransformerHelper() = default;

void TransformerHelper::Init() {
  SetTransform(gfx::Transform());
}

gfx::Insets TransformerHelper::GetHostInsets() const {
  return transformer_->GetHostInsets();
}

void TransformerHelper::SetTransform(const gfx::Transform& transform) {
  std::unique_ptr<RootWindowTransformer> transformer(
      new SimpleRootWindowTransformer(ash_host_->AsWindowTreeHost()->window(),
                                      transform));
  SetRootWindowTransformer(std::move(transformer));
}

void TransformerHelper::SetRootWindowTransformer(
    std::unique_ptr<RootWindowTransformer> transformer) {
  transformer_ = std::move(transformer);
  aura::WindowTreeHost* host = ash_host_->AsWindowTreeHost();
  aura::Window* window = host->window();
  window->SetTransform(transformer_->GetInsetsAndScaleTransform());
  // If the layer is not animating with a transform animation, then we need to
  // update the root window size immediately.
  if (!window->layer()->GetAnimator()->IsAnimatingProperty(
          ui::LayerAnimationElement::TRANSFORM)) {
    ash_host_->UpdateRootWindowSize();
  }
}

gfx::Transform TransformerHelper::GetTransform() const {
  float scale =
      ash_host_->AsWindowTreeHost()->window()->layer()->device_scale_factor();
  gfx::Transform transform;
  transform.Scale(scale, scale);
  transform *= transformer_->GetTransform();
  return transform;
}

gfx::Transform TransformerHelper::GetInverseTransform() const {
  float scale =
      ash_host_->AsWindowTreeHost()->window()->layer()->device_scale_factor();
  gfx::Transform transform;
  transform.Scale(1.0f / scale, 1.0f / scale);
  return transformer_->GetInverseTransform() * transform;
}

gfx::Rect TransformerHelper::GetTransformedWindowBounds(
    const gfx::Size& host_size) const {
  return transformer_->GetRootWindowBounds(host_size);
}

}  // namespace ash
