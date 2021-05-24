// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/resize_shadow_controller.h"

#include <memory>

#include "ash/wm/resize_shadow.h"

namespace ash {

ResizeShadowController::ResizeShadowController() = default;

ResizeShadowController::~ResizeShadowController() {
  HideAllShadows();
}

void ResizeShadowController::ShowShadow(aura::Window* window, int hit_test) {
  ResizeShadow* shadow = GetShadowForWindow(window);
  if (!shadow)
    shadow = CreateShadow(window);
  shadow->ShowForHitTest(hit_test);
}

void ResizeShadowController::HideShadow(aura::Window* window) {
  ResizeShadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->Hide();
}

void ResizeShadowController::HideAllShadows() {
  windows_observation_.RemoveAllObservations();
  window_shadows_.clear();
}

void ResizeShadowController::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  ResizeShadow* shadow = GetShadowForWindow(params.target);
  if (shadow)
    shadow->ReparentLayer();
}

void ResizeShadowController::OnWindowVisibilityChanging(aura::Window* window,
                                                        bool visible) {
  if (!visible)
    HideShadow(window);
}

void ResizeShadowController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  ResizeShadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->UpdateBoundsAndVisibility();
}

void ResizeShadowController::OnWindowStackingChanged(aura::Window* window) {
  ResizeShadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->ReparentLayer();
}

void ResizeShadowController::OnWindowDestroying(aura::Window* window) {
  windows_observation_.RemoveObservation(window);
  window_shadows_.erase(window);
}

ResizeShadow* ResizeShadowController::CreateShadow(aura::Window* window) {
  auto shadow = std::make_unique<ResizeShadow>(window);
  windows_observation_.AddObservation(window);

  ResizeShadow* raw_shadow = shadow.get();
  window_shadows_.insert(std::make_pair(window, std::move(shadow)));
  return raw_shadow;
}

ResizeShadow* ResizeShadowController::GetShadowForWindowForTest(
    aura::Window* window) {
  return GetShadowForWindow(window);
}

ResizeShadow* ResizeShadowController::GetShadowForWindow(aura::Window* window) {
  auto it = window_shadows_.find(window);
  return it != window_shadows_.end() ? it->second.get() : nullptr;
}

}  // namespace ash
