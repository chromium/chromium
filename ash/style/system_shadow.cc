// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_shadow.h"

#include "ash/root_window_controller.h"
#include "ash/style/style_util.h"
#include "base/check.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer_nine_patch.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_shadow.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

////////////////////////////////////////////////////////////////////////////////
// SystemShadowImpl:

// An implementation of `SystemShadow`. It is directly based on ui::Shadow.
class SystemShadowImpl : public SystemShadow, public ui::LayerOwner::Observer {
 public:
  SystemShadowImpl(SystemShadow::Type type,
                   const LayerRecreatedCallback& layer_recreated_callback)
      : layer_recreated_callback_(layer_recreated_callback) {
    shadow_.Init(SystemShadow::GetElevationFromType(type));
    shadow_.SetStyle(ui::Shadow::Style::kChromeOSSystemUI);

    if (layer_recreated_callback) {
      shadow_observation_.Observe(&shadow_);
    }
  }

  SystemShadowImpl(const SystemShadowImpl&) = delete;
  SystemShadowImpl& operator=(const SystemShadowImpl&) = delete;

  ~SystemShadowImpl() override = default;

  // ui::LayerOwner::Observer:
  void OnLayerRecreated(ui::Layer* old_layer) override {
    layer_recreated_callback_.Run(old_layer, shadow_.layer());
  }

 private:
  // SystemShadow:
  ui::Shadow* shadow() override { return &shadow_; }
  const ui::Shadow* shadow() const override { return &shadow_; }

  LayerRecreatedCallback layer_recreated_callback_;
  ui::Shadow shadow_;

  base::ScopedObservation<ui::LayerOwner, SystemShadowImpl> shadow_observation_{
      this};
};

////////////////////////////////////////////////////////////////////////////////
// SystemViewShadow:

// An implementation of `SystemShadow`. It is based on ViewShadow. The
// ViewShadow is added in the layers beneath the view and adjusts its content
// bounds with the view's bounds. Do not manually set the content bounds.
class SystemViewShadow : public SystemShadow, public views::ViewObserver {
 public:
  SystemViewShadow(views::View* view, SystemShadow::Type type)
      : view_shadow_(view, SystemShadow::GetElevationFromType(type)) {
    view_shadow_.shadow()->SetStyle(ui::Shadow::Style::kChromeOSSystemUI);
    view_observation_.Observe(view);
    if (auto* widget = view->GetWidget()) {
      ObserveColorProviderSource(widget);
    }
  }

  SystemViewShadow(const SystemViewShadow&) = delete;
  SystemViewShadow& operator=(const SystemViewShadow&) = delete;

  ~SystemViewShadow() override = default;

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override {
    ObserveColorProviderSource(observed_view->GetWidget());
  }
  void OnViewIsDeleting(views::View* observed_view) override {
    view_observation_.Reset();
  }

 private:
  // SystemShadow:
  ui::Shadow* shadow() override { return view_shadow_.shadow(); }
  const ui::Shadow* shadow() const override { return view_shadow_.shadow(); }

  views::ViewShadow view_shadow_;
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
};

////////////////////////////////////////////////////////////////////////////////
// SystemWindowShadow:

// An extension of SystemShadowImpl. The shadow is added at the bottom of a
// window's layer and adjusts its content bounds with the window's bounds. Do
// not manually set the content bounds.
class SystemWindowShadow : public SystemShadowImpl,
                           public aura::WindowObserver {
 public:
  SystemWindowShadow(aura::Window* window, SystemShadow::Type type)
      : SystemShadowImpl(type, LayerRecreatedCallback()) {
    auto* window_layer = window->layer();
    auto* shadow_layer = GetLayer();
    window_layer->Add(shadow_layer);
    window_layer->StackAtBottom(shadow_layer);
    SetContentBounds(window_layer->bounds());

    window_observation_.Observe(window);

    if (window->GetRootWindow()) {
      ObserveColorProviderSource(
          RootWindowController::ForWindow(window)->color_provider_source());
    }
  }

  SystemWindowShadow(const SystemWindowShadow&) = delete;
  SystemWindowShadow& operator=(const SystemWindowShadow&) = delete;

  ~SystemWindowShadow() override = default;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    SetContentBounds(gfx::Rect(new_bounds.size()));
  }
  void OnWindowDestroyed(aura::Window* window) override {
    window_observation_.Reset();
  }
  void OnWindowAddedToRootWindow(aura::Window* window) override {
    ObserveColorProviderSource(
        RootWindowController::ForWindow(window)->color_provider_source());
  }

 private:
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace

SystemShadow::~SystemShadow() = default;

// static
std::unique_ptr<SystemShadow> SystemShadow::CreateShadowOnNinePatchLayer(
    Type shadow_type,
    const LayerRecreatedCallback& layer_recreated_callback) {
  return std::make_unique<SystemShadowImpl>(shadow_type,
                                            layer_recreated_callback);
}

// static
std::unique_ptr<SystemShadow> SystemShadow::CreateShadowOnNinePatchLayerForView(
    views::View* view,
    Type shadow_type) {
  DCHECK(view);
  return std::make_unique<SystemViewShadow>(view, shadow_type);
}

// static
std::unique_ptr<SystemShadow>
SystemShadow::CreateShadowOnNinePatchLayerForWindow(aura::Window* window,
                                                    Type shadow_type) {
  DCHECK(window);
  return std::make_unique<SystemWindowShadow>(window, shadow_type);
}

// static
int SystemShadow::GetElevationFromType(Type type) {
  switch (type) {
    case Type::kElevation4:
      return 4;
    case Type::kElevation12:
      return 12;
    case Type::kElevation24:
      return 24;
  }
}

void SystemShadow::SetType(SystemShadow::Type type) {
  shadow()->SetElevation(SystemShadow::GetElevationFromType(type));
}

void SystemShadow::SetContentBounds(const gfx::Rect& bounds) {
  shadow()->SetContentBounds(bounds);
}

void SystemShadow::SetRoundedCorners(
    const gfx::RoundedCornersF& rounded_corners) {
  shadow()->SetRoundedCorners(rounded_corners);
}

const gfx::Rect& SystemShadow::GetContentBounds() {
  return shadow()->content_bounds();
}

ui::Layer* SystemShadow::GetLayer() {
  return shadow()->layer();
}

ui::LayerNinePatch* SystemShadow::GetNinePatchLayer() {
  return shadow()->shadow_layer();
}

void SystemShadow::ObserveColorProviderSource(
    ui::ColorProviderSource* color_provider_source) {
  Observe(color_provider_source);
}

void SystemShadow::OnColorProviderChanged() {
  if (auto* color_provider_source = GetColorProviderSource()) {
    UpdateShadowColors(color_provider_source->GetColorProvider());
  }
}

const gfx::ShadowValues SystemShadow::GetShadowValuesForTesting() const {
  return shadow()->details_for_testing()->values;  // IN-TEST
}

void SystemShadow::UpdateShadowColors(const ui::ColorProvider* color_provider) {
  shadow()->SetColorMap(
      StyleUtil::CreateShadowElevationToColorsMap(color_provider));
}

}  // namespace ash
