// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_shadow_on_nine_patch_layer.h"

#include "ash/root_window_controller.h"
#include "ash/style/ash_color_provider_source.h"
#include "ash/style/style_util.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

// -----------------------------------------------------------------------------
// SystemShadowOnNinePatchLayer:
SystemShadowOnNinePatchLayer::~SystemShadowOnNinePatchLayer() = default;

void SystemShadowOnNinePatchLayer::SetType(SystemShadow::Type type) {
  shadow()->SetElevation(SystemShadow::GetElevationFromType(type));
}

void SystemShadowOnNinePatchLayer::SetContentBounds(const gfx::Rect& bounds) {
  shadow()->SetContentBounds(bounds);
}

void SystemShadowOnNinePatchLayer::SetRoundedCornerRadius(int corner_radius) {
  shadow()->SetRoundedCornerRadius(corner_radius);
}

void SystemShadowOnNinePatchLayer::SetRoundedCorners(
    const gfx::RoundedCornersF& rounded_corners) {
  // TODO(http://b/307326019): use corresponding interface of `ui::Shadow` when
  // available.
  NOTREACHED() << "Setting uneven rounded corners to the shadow on nine patch "
                  "layer is not ready.";
}

const gfx::Rect& SystemShadowOnNinePatchLayer::GetContentBounds() {
  return shadow()->content_bounds();
}

ui::Layer* SystemShadowOnNinePatchLayer::GetLayer() {
  return shadow()->layer();
}

ui::Layer* SystemShadowOnNinePatchLayer::GetNinePatchLayer() {
  return shadow()->shadow_layer();
}

const gfx::ShadowValues
SystemShadowOnNinePatchLayer::GetShadowValuesForTesting() const {
  return shadow()->details_for_testing()->values;
}

void SystemShadowOnNinePatchLayer::UpdateShadowColors(
    const ui::ColorProvider* color_provider) {
  shadow()->SetElevationToColorsMap(
      StyleUtil::CreateShadowElevationToColorsMap(color_provider));
}

// -----------------------------------------------------------------------------
// SystemShadowOnNinePatchLayerImpl:
SystemShadowOnNinePatchLayerImpl::SystemShadowOnNinePatchLayerImpl(
    SystemShadow::Type type,
    const LayerRecreatedCallback& layer_recreated_callback)
    : layer_recreated_callback_(layer_recreated_callback) {
  shadow_.Init(SystemShadow::GetElevationFromType(type));
  shadow_.SetShadowStyle(gfx::ShadowStyle::kChromeOSSystemUI);

  if (layer_recreated_callback) {
    shadow_observation_.Observe(&shadow_);
  }
}

SystemShadowOnNinePatchLayerImpl::~SystemShadowOnNinePatchLayerImpl() = default;

void SystemShadowOnNinePatchLayerImpl::OnLayerRecreated(ui::Layer* old_layer) {
  layer_recreated_callback_.Run(old_layer, shadow_.layer());
}

ui::Shadow* SystemShadowOnNinePatchLayerImpl::shadow() {
  return &shadow_;
}

const ui::Shadow* SystemShadowOnNinePatchLayerImpl::shadow() const {
  return &shadow_;
}

// -----------------------------------------------------------------------------
// SystemViewShadowOnNinePatchLayer:
SystemViewShadowOnNinePatchLayer::SystemViewShadowOnNinePatchLayer(
    views::View* view,
    SystemShadow::Type type)
    : view_shadow_(view, SystemShadow::GetElevationFromType(type)) {
  view_shadow_.shadow()->SetShadowStyle(gfx::ShadowStyle::kChromeOSSystemUI);
  view_observation_.Observe(view);
  if (auto* widget = view->GetWidget()) {
    ObserveColorProviderSource(widget);
  }
}

SystemViewShadowOnNinePatchLayer::~SystemViewShadowOnNinePatchLayer() = default;

void SystemViewShadowOnNinePatchLayer::SetRoundedCornerRadius(
    int corner_radius) {
  view_shadow_.SetRoundedCornerRadius(corner_radius);
}

void SystemViewShadowOnNinePatchLayer::OnViewAddedToWidget(
    views::View* observed_view) {
  ObserveColorProviderSource(observed_view->GetWidget());
}

void SystemViewShadowOnNinePatchLayer::OnViewIsDeleting(
    views::View* observed_view) {
  view_observation_.Reset();
}

void SystemViewShadowOnNinePatchLayer::SetContentBounds(
    const gfx::Rect& content_bounds) {}

ui::Shadow* SystemViewShadowOnNinePatchLayer::shadow() {
  return view_shadow_.shadow();
}

const ui::Shadow* SystemViewShadowOnNinePatchLayer::shadow() const {
  return view_shadow_.shadow();
}

// -----------------------------------------------------------------------------
// SystemWindowShadowOnNinePatchLayer:
SystemWindowShadowOnNinePatchLayer::SystemWindowShadowOnNinePatchLayer(
    aura::Window* window,
    SystemShadow::Type type)
    : SystemShadowOnNinePatchLayerImpl(type, LayerRecreatedCallback()) {
  auto* window_layer = window->layer();
  auto* shadow_layer = GetLayer();
  window_layer->Add(shadow_layer);
  window_layer->StackAtBottom(shadow_layer);
  SystemShadowOnNinePatchLayerImpl::SetContentBounds(window_layer->bounds());

  window_observation_.Observe(window);

  if (window->GetRootWindow()) {
    ObserveColorProviderSource(
        RootWindowController::ForWindow(window)->color_provider_source());
  }
}

SystemWindowShadowOnNinePatchLayer::~SystemWindowShadowOnNinePatchLayer() =
    default;

void SystemWindowShadowOnNinePatchLayer::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  SystemShadowOnNinePatchLayerImpl::SetContentBounds(
      gfx::Rect(new_bounds.size()));
}

void SystemWindowShadowOnNinePatchLayer::OnWindowDestroyed(
    aura::Window* window) {
  window_observation_.Reset();
}

void SystemWindowShadowOnNinePatchLayer::OnWindowAddedToRootWindow(
    aura::Window* window) {
  ObserveColorProviderSource(
      RootWindowController::ForWindow(window)->color_provider_source());
}

void SystemWindowShadowOnNinePatchLayer::SetContentBounds(
    const gfx::Rect& content_bounds) {}

}  // namespace ash
