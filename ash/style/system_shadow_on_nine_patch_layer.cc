// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_shadow_on_nine_patch_layer.h"

#include "ui/compositor/layer.h"

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

const gfx::Rect& SystemShadowOnNinePatchLayer::GetContentBounds() {
  return shadow()->content_bounds();
}

ui::Layer* SystemShadowOnNinePatchLayer::GetLayer() {
  return shadow()->layer();
}

ui::Layer* SystemShadowOnNinePatchLayer::GetNinePatchLayer() {
  return shadow()->shadow_layer();
}

// -----------------------------------------------------------------------------
// SystemShadowOnNinePatchLayerImpl:
SystemShadowOnNinePatchLayerImpl::SystemShadowOnNinePatchLayerImpl(
    SystemShadow::Type type) {
  shadow_.Init(SystemShadow::GetElevationFromType(type));
  shadow_.SetShadowStyle(gfx::ShadowStyle::kChromeOSSystemUI);
}

SystemShadowOnNinePatchLayerImpl::~SystemShadowOnNinePatchLayerImpl() = default;

ui::Shadow* SystemShadowOnNinePatchLayerImpl::shadow() {
  return &shadow_;
}

// -----------------------------------------------------------------------------
// SystemViewShadowOnNinePatchLayer:
SystemViewShadowOnNinePatchLayer::SystemViewShadowOnNinePatchLayer(
    views::View* view,
    SystemShadow::Type type)
    : view_shadow_(view, SystemShadow::GetElevationFromType(type)) {
  view_shadow_.shadow()->SetShadowStyle(gfx::ShadowStyle::kChromeOSSystemUI);
}

SystemViewShadowOnNinePatchLayer::~SystemViewShadowOnNinePatchLayer() = default;

void SystemViewShadowOnNinePatchLayer::SetRoundedCornerRadius(
    int corner_radius) {
  view_shadow_.SetRoundedCornerRadius(corner_radius);
}

void SystemViewShadowOnNinePatchLayer::SetContentBounds(
    const gfx::Rect& content_bounds) {}

ui::Shadow* SystemViewShadowOnNinePatchLayer::shadow() {
  return view_shadow_.shadow();
}

// -----------------------------------------------------------------------------
// SystemWindowShadowOnNinePatchLayer:
SystemWindowShadowOnNinePatchLayer::SystemWindowShadowOnNinePatchLayer(
    aura::Window* window,
    SystemShadow::Type type)
    : SystemShadowOnNinePatchLayerImpl(type) {
  auto* window_layer = window->layer();
  auto* shadow_layer = GetLayer();
  window_layer->Add(shadow_layer);
  window_layer->StackAtBottom(shadow_layer);
  SystemShadowOnNinePatchLayerImpl::SetContentBounds(window_layer->bounds());

  window_observation_.Observe(window);
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

void SystemWindowShadowOnNinePatchLayer::SetContentBounds(
    const gfx::Rect& content_bounds) {}

}  // namespace ash
