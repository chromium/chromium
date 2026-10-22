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
// SystemShadowImpl:

// An implementation of `SystemShadow`. It directly owns a shadow Decoration.
class SystemShadowImpl : public SystemShadow {
 public:
  explicit SystemShadowImpl(SystemShadow::Type type)
      : decoration_(ui::Decoration::CreateShadow(
            SystemShadow::GetElevationFromType(type),
            ui::decoration::Shadow::Style::kChromeOSSystemUI)) {}

  SystemShadowImpl(const SystemShadowImpl&) = delete;
  SystemShadowImpl& operator=(const SystemShadowImpl&) = delete;

  ~SystemShadowImpl() override = default;

 private:
  // SystemShadow:
  ui::Decoration* decoration() override { return decoration_.get(); }
  const ui::Decoration* decoration() const override {
    return decoration_.get();
  }

  std::unique_ptr<ui::Decoration> decoration_;
};

////////////////////////////////////////////////////////////////////////////////
// SystemViewShadow:

// An implementation of `SystemShadow`. It is based on ViewDecoration. The
// ViewDecoration is added in the layers beneath the view and adjusts its
// content bounds with the view's bounds. Do not manually set the content
// bounds.
class SystemViewShadow : public SystemShadow, public views::ViewObserver {
 public:
  SystemViewShadow(views::View* view, SystemShadow::Type type)
      : view_decoration_(view) {
    view_decoration_.SetShadow(
        SystemShadow::GetElevationFromType(type),
        ui::decoration::Shadow::Style::kChromeOSSystemUI);
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
// SystemWindowShadow:

// An extension of SystemShadowImpl. The shadow is added at the bottom of a
// window's layer and adjusts its content bounds with the window's bounds. Do
// not manually set the content bounds.
class SystemWindowShadow : public SystemShadowImpl,
                           public aura::WindowObserver {
 public:
  SystemWindowShadow(aura::Window* window, SystemShadow::Type type)
      : SystemShadowImpl(type) {
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
    Type shadow_type) {
  return std::make_unique<SystemShadowImpl>(shadow_type);
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
  GetShadow(decoration())
      ->SetElevation(SystemShadow::GetElevationFromType(type));
}

void SystemShadow::SetContentBounds(const gfx::Rect& bounds) {
  decoration()->SetContentBounds(bounds);
}

void SystemShadow::SetRoundedCorners(
    const gfx::RoundedCornersF& rounded_corners) {
  decoration()->SetRoundedCorners(rounded_corners);
}

const gfx::Rect& SystemShadow::GetContentBounds() {
  return decoration()->content_bounds();
}

ui::Layer* SystemShadow::GetLayer() {
  return decoration()->layer();
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
  return GetShadow(decoration())
      ->GetSpec(decoration()->content_bounds(), decoration()->rounded_corners())
      .value_or(gfx::ShadowValues());
}

void SystemShadow::UpdateShadowColors(const ui::ColorProvider* color_provider) {
  GetShadow(decoration())
      ->SetColorMap(
          StyleUtil::CreateShadowElevationToColorsMap(color_provider));
}

}  // namespace ash
