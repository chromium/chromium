// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/resize_shadow_controller.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/resize_shadow.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "base/time/time.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_source.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/decoration/decoration.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ash {

namespace {

constexpr base::TimeDelta kHideDuration = base::Milliseconds(100);

}  // namespace

// Owns a window's resize shadow `ui::Decoration` and keeps its color updated
// from the window's root `ui::ColorProviderSource`.
class ResizeShadowController::WindowResizeShadow
    : public ui::ColorProviderSourceObserver {
 public:
  WindowResizeShadow(std::unique_ptr<ui::Decoration> decoration,
                     ui::ColorProviderSource* color_provider_source)
      : decoration_(std::move(decoration)) {
    if (shadow()->type() == ResizeShadowType::kLock) {
      shadow()->SetColor(gfx::kGoogleGrey900);
    } else {
      ObserveColorProviderSource(color_provider_source);
    }
  }

  WindowResizeShadow(const WindowResizeShadow&) = delete;
  WindowResizeShadow& operator=(const WindowResizeShadow&) = delete;
  ~WindowResizeShadow() override = default;

  ui::Decoration* decoration() const { return decoration_.get(); }
  ResizeShadow* shadow() const {
    return decoration_->GetSourceAs<ResizeShadow>();
  }

  void ObserveColorProviderSource(
      ui::ColorProviderSource* color_provider_source) {
    if (shadow()->type() != ResizeShadowType::kLock) {
      Observe(color_provider_source);
    }
  }

  // Updates bounds, opacity, and visibility of the shadow layer.
  void UpdateBoundsAndVisibility(const gfx::Rect& window_bounds) {
    ui::Layer* layer = decoration_->layer();
    if (!shadow()->visible() && !layer->GetTargetVisibility()) {
      return;
    }

    if (shadow()->visible()) {
      decoration_->SetContentBounds(window_bounds);
    }

    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    if (!shadow()->visible()) {
      settings.SetTransitionDuration(shadow()->type() == ResizeShadowType::kLock
                                         ? base::TimeDelta()
                                         : kHideDuration);
    }
    layer->SetOpacity(shadow()->visible() ? shadow()->params().opacity : 0.f);
    layer->SetVisible(shadow()->visible());
  }

  // ui::ColorProviderSourceObserver:
  void OnColorProviderChanged() override {
    const ui::ColorProviderSource* source = GetColorProviderSource();
    if (!source) {
      return;
    }
    if (const ui::ColorProvider* color_provider = source->GetColorProvider()) {
      shadow()->SetColor(color_provider->GetColor(kColorAshResizeShadowColor));
    }
  }

 private:
  std::unique_ptr<ui::Decoration> decoration_;
};

ResizeShadowController::ResizeShadowController() = default;

ResizeShadowController::~ResizeShadowController() {
  RemoveAllShadows();
}

void ResizeShadowController::ShowShadow(aura::Window* window, int hit_test) {
  if (ShouldShowShadowForWindow(window) && window->IsVisible()) {
    RecreateShadowIfNeeded(window);
    GetShadowForWindow(window)->ShowForHitTest(hit_test);
    UpdateShadowBoundsAndVisibility(window, window->bounds());
  }
}

void ResizeShadowController::TryShowAllShadows() {
  for (const auto& [window, shadow] : window_shadows_) {
    UpdateShadowVisibility(window, window->IsVisible());
  }
}

void ResizeShadowController::HideShadow(aura::Window* window) {
  if (!GetShadowForWindow(window)) {
    return;
  }
  UpdateShadowVisibility(window, false);
}

void ResizeShadowController::HideAllShadows() {
  for (auto& [window, window_shadow] : window_shadows_) {
    if (!window_shadow) {
      continue;
    }

    switch (window_shadow->shadow()->type()) {
      case ResizeShadowType::kLock:  // Hides lock style of shadow
        UpdateShadowVisibility(window, false);
        break;
      case ResizeShadowType::kUnlock:  // Deletes unlock style of shadow
        window_shadow.reset();
        break;
    }
  }
}

void ResizeShadowController::OnCrossFadeAnimationCompleted(
    aura::Window* window) {
  ReparentShadowLayer(window);
}

void ResizeShadowController::RemoveAllShadows() {
  windows_observation_.RemoveAllObservations();
  window_shadows_.clear();
}

void ResizeShadowController::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  ReparentShadowLayer(params.target);
}

void ResizeShadowController::OnWindowVisibilityChanging(aura::Window* window,
                                                        bool visible) {
  UpdateShadowVisibility(window, visible);
}

void ResizeShadowController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (GetShadowForWindow(window) &&
      window->GetProperty(aura::client::kUseWindowBoundsForShadow)) {
    UpdateShadowBoundsAndVisibility(window, new_bounds);
  }
}

void ResizeShadowController::OnWindowStackingChanged(aura::Window* window) {
  ReparentShadowLayer(window);
}

void ResizeShadowController::OnWindowDestroying(aura::Window* window) {
  windows_observation_.RemoveObservation(window);
  window_shadows_.erase(window);
}

void ResizeShadowController::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (key == aura::client::kShowStateKey) {
    UpdateShadowVisibility(window, window->IsVisible());
    return;
  }

  // If the resize shadow is being shown, ensure that shadow is configured
  // correctly for either a rounded window or squared window.
  if (ShouldShowShadowForWindow(window) &&
      chromeos::CanPropertyEffectWindowRoundedCorners(key)) {
    RecreateShadowIfNeeded(window);
    UpdateShadowVisibility(window, window->IsVisible());
    return;
  }
}

void ResizeShadowController::OnWindowAddedToRootWindow(aura::Window* window) {
  if (WindowResizeShadow* window_shadow = GetWindowResizeShadow(window)) {
    RootWindowController* root_controller =
        RootWindowController::ForWindow(window);
    window_shadow->ObserveColorProviderSource(
        root_controller ? root_controller->color_provider_source() : nullptr);
  }
}

void ResizeShadowController::UpdateResizeShadowBoundsOfWindow(
    aura::Window* window,
    const gfx::Rect& bounds) {
  UpdateShadowBoundsAndVisibility(window, bounds);
}

ResizeShadow* ResizeShadowController::GetShadowForWindowForTest(
    aura::Window* window) {
  return GetShadowForWindow(window);
}

ui::Decoration* ResizeShadowController::GetDecorationForWindowForTest(
    aura::Window* window) {
  WindowResizeShadow* window_shadow = GetWindowResizeShadow(window);
  return window_shadow ? window_shadow->decoration() : nullptr;
}

ui::ColorProviderSourceObserver*
ResizeShadowController::GetColorObserverForWindowForTest(aura::Window* window) {
  return GetWindowResizeShadow(window);
}

void ResizeShadowController::RecreateShadowIfNeeded(aura::Window* window) {
  if (!windows_observation_.IsObservingSource(window)) {
    windows_observation_.AddObservation(window);
  }
  ResizeShadow* shadow = GetShadowForWindow(window);
  const ash::ResizeShadowType type =
      window->GetProperty(ash::kResizeShadowTypeKey);
  const gfx::RoundedCornersF window_radii =
      ash::WindowState::Get(window)->GetWindowRoundedCorners();
  const bool has_large_rounded_corners =
      window_radii.upper_left() > chromeos::kRoundedWindowSmallCornerRadius ||
      window_radii.upper_right() > chromeos::kRoundedWindowSmallCornerRadius ||
      window_radii.lower_right() > chromeos::kRoundedWindowSmallCornerRadius ||
      window_radii.lower_left() > chromeos::kRoundedWindowSmallCornerRadius;
  const gfx::RoundedCornersF target_radii =
      has_large_rounded_corners ? window_radii : gfx::RoundedCornersF(2.f);

  // If the `window` has a resize shadow with the requested type and the shadow
  // is configured for small/large rounded corners, update its corner radii in
  // place without recreating it.
  if (shadow && shadow->type() == type &&
      shadow->is_for_large_rounded_corners() == has_large_rounded_corners) {
    GetWindowResizeShadow(window)->decoration()->SetRoundedCorners(
        target_radii);
    return;
  }

  ResizeShadow::InitParams params;
  if (type == ResizeShadowType::kLock) {
    params.thickness = 6;
    params.shadow_corner_radius = 6;
    params.opacity = 0.3f;
    params.hit_test_enabled = false;
    params.is_for_large_rounded_corners = false;
  }

  // Configure shadow thickness and corner radius when `window` has large
  // rounded corners.
  if (has_large_rounded_corners) {
    params.thickness = 6;
    params.shadow_corner_radius = 16;
    params.is_for_large_rounded_corners = true;
  }

  auto new_shadow = std::make_unique<ResizeShadow>(params, type);
  auto decoration = ui::Decoration::Create(std::move(new_shadow),
                                           /*debug_name=*/"ResizeShadow");
  decoration->SetRoundedCorners(target_radii);
  decoration->layer()->SetOpacity(0.f);
  decoration->layer()->SetVisible(false);

  RootWindowController* root_controller =
      RootWindowController::ForWindow(window);
  window_shadows_.insert_or_assign(
      window, std::make_unique<WindowResizeShadow>(
                  std::move(decoration),
                  root_controller ? root_controller->color_provider_source()
                                  : nullptr));
  ReparentShadowLayer(window);
}

ResizeShadowController::WindowResizeShadow*
ResizeShadowController::GetWindowResizeShadow(aura::Window* window) const {
  auto it = window_shadows_.find(window);
  return it != window_shadows_.end() ? it->second.get() : nullptr;
}

ResizeShadow* ResizeShadowController::GetShadowForWindow(
    aura::Window* window) const {
  WindowResizeShadow* window_shadow = GetWindowResizeShadow(window);
  return window_shadow ? window_shadow->shadow() : nullptr;
}

void ResizeShadowController::ReparentShadowLayer(aura::Window* window) const {
  WindowResizeShadow* window_shadow = GetWindowResizeShadow(window);
  if (!window_shadow || !window->layer()->parent()) {
    return;
  }

  ui::Layer* layer = window_shadow->decoration()->layer();
  if (layer->parent() != window->layer()->parent()) {
    window->layer()->parent()->Add(layer);
  }
  layer->parent()->StackBelow(layer, window->layer());
}

void ResizeShadowController::UpdateShadowBoundsAndVisibility(
    aura::Window* window,
    const gfx::Rect& window_bounds) const {
  if (WindowResizeShadow* window_shadow = GetWindowResizeShadow(window)) {
    window_shadow->UpdateBoundsAndVisibility(window_bounds);
  }
}

void ResizeShadowController::UpdateShadowVisibility(aura::Window* window,
                                                    bool visible) const {
  ResizeShadow* shadow = GetShadowForWindow(window);
  if (!shadow) {
    return;
  }

  if (shadow->type() == ResizeShadowType::kLock) {
    visible &= ShouldShowShadowForWindow(window);
    if (visible) {
      shadow->ShowForHitTest();
      UpdateShadowBoundsAndVisibility(window, window->bounds());
    }
  }

  if (!visible) {
    shadow->Hide();
    UpdateShadowBoundsAndVisibility(window, window->bounds());
  }
}

bool ResizeShadowController::ShouldShowShadowForWindow(
    aura::Window* window) const {
  if (window->GetProperty(kDisableResizeShadow) ||
      Shell::Get()->overview_controller()->InOverviewSession()) {
    return false;
  }

  return chromeos::ShouldShowResizeBorder(window);
}

}  // namespace ash
