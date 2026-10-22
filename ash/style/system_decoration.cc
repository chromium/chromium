// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_decoration.h"

#include "ash/root_window_controller.h"
#include "ash/style/style_util.h"
#include "base/check.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer_nine_patch.h"
#include "ui/decoration/decoration.h"
#include "ui/decoration/shadow.h"
#include "ui/decoration/shadow_with_highlight_border.h"
#include "ui/views/view.h"
#include "ui/views/view_decoration.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

ui::decoration::Shadow* GetShadow(ui::Decoration* decoration) {
  if (auto* shadow = decoration->GetSourceAs<ui::decoration::Shadow>()) {
    return shadow;
  }
  if (auto* shadow_with_border =
          decoration
              ->GetSourceAs<ui::decoration::ShadowWithHighlightBorder>()) {
    return shadow_with_border->shadow();
  }
  return nullptr;
}

const ui::decoration::Shadow* GetShadow(const ui::Decoration* decoration) {
  if (const auto* shadow = decoration->GetSourceAs<ui::decoration::Shadow>()) {
    return shadow;
  }
  if (const auto* shadow_with_border =
          decoration
              ->GetSourceAs<ui::decoration::ShadowWithHighlightBorder>()) {
    return shadow_with_border->shadow();
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// SystemDecorationImpl:

// An implementation of `SystemDecoration`. It directly owns a shadow
// Decoration.
class SystemDecorationImpl : public SystemDecoration {
 public:
  explicit SystemDecorationImpl(SystemDecoration::ShadowType type)
      : decoration_(ui::Decoration::CreateShadow(
            SystemDecoration::GetElevationFromType(type),
            ui::decoration::Shadow::Style::kChromeOSSystemUI)) {}

  SystemDecorationImpl(const SystemDecorationImpl&) = delete;
  SystemDecorationImpl& operator=(const SystemDecorationImpl&) = delete;

  ~SystemDecorationImpl() override = default;

 private:
  // SystemDecoration:
  ui::Decoration* decoration() override { return decoration_.get(); }
  const ui::Decoration* decoration() const override {
    return decoration_.get();
  }

  std::unique_ptr<ui::Decoration> decoration_;
};

////////////////////////////////////////////////////////////////////////////////
// SystemViewDecoration:

// An implementation of `SystemDecoration`. It is based on ViewDecoration. The
// ViewDecoration is added in the layers beneath the view and adjusts its
// content bounds with the view's bounds. Do not manually set the content
// bounds.
class SystemViewDecoration : public SystemDecoration,
                             public views::ViewObserver {
 public:
  SystemViewDecoration(views::View* view, SystemDecoration::ShadowType type)
      : view_decoration_(view) {
    view_decoration_.SetShadow(
        SystemDecoration::GetElevationFromType(type),
        ui::decoration::Shadow::Style::kChromeOSSystemUI);
    view_observation_.Observe(view);
    if (auto* widget = view->GetWidget()) {
      ObserveColorProviderSource(widget);
    }
  }

  SystemViewDecoration(const SystemViewDecoration&) = delete;
  SystemViewDecoration& operator=(const SystemViewDecoration&) = delete;

  ~SystemViewDecoration() override = default;

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override {
    ObserveColorProviderSource(observed_view->GetWidget());
  }
  void OnViewIsDeleting(views::View* observed_view) override {
    view_observation_.Reset();
  }

 private:
  // SystemDecoration:
  ui::Decoration* decoration() override {
    return view_decoration_.decoration();
  }
  const ui::Decoration* decoration() const override {
    return view_decoration_.decoration();
  }

  views::ViewDecoration view_decoration_;
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
};

////////////////////////////////////////////////////////////////////////////////
// SystemWindowDecoration:

// An extension of SystemDecorationImpl. The shadow is added at the bottom of a
// window's layer and adjusts its content bounds with the window's bounds. Do
// not manually set the content bounds.
class SystemWindowDecoration : public SystemDecorationImpl,
                               public aura::WindowObserver {
 public:
  SystemWindowDecoration(aura::Window* window,
                         SystemDecoration::ShadowType type)
      : SystemDecorationImpl(type) {
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

  SystemWindowDecoration(const SystemWindowDecoration&) = delete;
  SystemWindowDecoration& operator=(const SystemWindowDecoration&) = delete;

  ~SystemWindowDecoration() override = default;

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

SystemDecoration::~SystemDecoration() = default;

// static
std::unique_ptr<SystemDecoration>
SystemDecoration::CreateShadowOnNinePatchLayer(ShadowType shadow_type) {
  return std::make_unique<SystemDecorationImpl>(shadow_type);
}

// static
std::unique_ptr<SystemDecoration>
SystemDecoration::CreateShadowOnNinePatchLayerForView(views::View* view,
                                                      ShadowType shadow_type) {
  DCHECK(view);
  return std::make_unique<SystemViewDecoration>(view, shadow_type);
}

// static
std::unique_ptr<SystemDecoration>
SystemDecoration::CreateShadowOnNinePatchLayerForWindow(
    aura::Window* window,
    ShadowType shadow_type) {
  DCHECK(window);
  return std::make_unique<SystemWindowDecoration>(window, shadow_type);
}

// static
int SystemDecoration::GetElevationFromType(ShadowType type) {
  switch (type) {
    case ShadowType::kElevation4:
      return 4;
    case ShadowType::kElevation12:
      return 12;
    case ShadowType::kElevation24:
      return 24;
  }
}

void SystemDecoration::SetType(SystemDecoration::ShadowType type) {
  GetShadow(decoration())
      ->SetElevation(SystemDecoration::GetElevationFromType(type));
}

void SystemDecoration::SetContentBounds(const gfx::Rect& bounds) {
  decoration()->SetContentBounds(bounds);
}

void SystemDecoration::SetRoundedCorners(
    const gfx::RoundedCornersF& rounded_corners) {
  decoration()->SetRoundedCorners(rounded_corners);
}

const gfx::Rect& SystemDecoration::GetContentBounds() {
  return decoration()->content_bounds();
}

ui::Layer* SystemDecoration::GetLayer() {
  return decoration()->layer();
}

void SystemDecoration::ObserveColorProviderSource(
    ui::ColorProviderSource* color_provider_source) {
  Observe(color_provider_source);
}

void SystemDecoration::OnColorProviderChanged() {
  if (auto* color_provider_source = GetColorProviderSource()) {
    UpdateShadowColors(color_provider_source->GetColorProvider());
  }
}

const gfx::ShadowValues SystemDecoration::GetShadowValuesForTesting() const {
  return GetShadow(decoration())
      ->GetSpec(decoration()->content_bounds(), decoration()->rounded_corners())
      .value_or(gfx::ShadowValues());
}

void SystemDecoration::UpdateShadowColors(
    const ui::ColorProvider* color_provider) {
  GetShadow(decoration())
      ->SetColorMap(
          StyleUtil::CreateShadowElevationToColorsMap(color_provider));
}

}  // namespace ash
