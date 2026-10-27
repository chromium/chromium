// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_decoration.h"

#include <memory>
#include <optional>

#include "ash/root_window_controller.h"
#include "ash/style/style_util.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer_nine_patch.h"
#include "ui/decoration/decoration.h"
#include "ui/decoration/highlight_border.h"
#include "ui/decoration/shadow.h"
#include "ui/decoration/shadow_with_highlight_border.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

ui::ColorId GetHighlightColorId(views::HighlightBorder::Type type) {
  switch (type) {
    case views::HighlightBorder::Type::kHighlightBorderNoShadow:
    case views::HighlightBorder::Type::kHighlightBorderOnShadow:
      return ui::kColorCrosSystemHighlight;
  }
}

ui::ColorId GetBorderColorId(views::HighlightBorder::Type type) {
  switch (type) {
    case views::HighlightBorder::Type::kHighlightBorderNoShadow:
      return ui::kColorCrosSystemHighlightBorder;
    case views::HighlightBorder::Type::kHighlightBorderOnShadow:
      return ui::kColorCrosSystemHighlightBorder1;
  }
}

////////////////////////////////////////////////////////////////////////////////
// SystemViewDecoration:

// An extension of `SystemDecoration` for a `views::View`. The decoration layer
// is added to the view's region layers and adjusts its content bounds with the
// view's bounds. Do not manually set the content bounds.
class SystemViewDecoration : public SystemDecoration,
                             public views::ViewObserver {
 public:
  explicit SystemViewDecoration(views::View* view) : view_(view) {
    if (!view_->layer()) {
      view_->SetPaintToLayer();
    }
    MoveLayerToRegion(views::LayerRegion::kBelow);
    view_observation_.Observe(view_);
    if (auto* widget = view_->GetWidget()) {
      ObserveColorProviderSource(widget);
    }
  }

  SystemViewDecoration(const SystemViewDecoration&) = delete;
  SystemViewDecoration& operator=(const SystemViewDecoration&) = delete;

  // Detach the decoration layer from `view_`'s regions before the base class
  // destroys it.
  ~SystemViewDecoration() override {
    if (view_) {
      OnViewIsDeleting(view_);
    }
  }

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override {
    ObserveColorProviderSource(observed_view->GetWidget());
  }
  void OnViewLayerBoundsSet(views::View* observed_view) override {
    SetContentBounds(observed_view->layer()->bounds());
  }
  void OnViewIsDeleting(views::View* observed_view) override {
    view_observation_.Reset();
    view_->RemoveLayerFromRegions(GetLayer());
    view_ = nullptr;
  }

 protected:
  // SystemDecoration:
  void OnHighlightBorderChanged() override {
    if (view_) {
      MoveLayerToRegion(HasHighlightBorder() ? views::LayerRegion::kAbove
                                             : views::LayerRegion::kBelow);
    }
  }

 private:
  void MoveLayerToRegion(views::LayerRegion region) {
    view_->RemoveLayerFromRegionsKeepInLayerTree(GetLayer());
    view_->AddLayerToRegion(GetLayer(), region);
    SetContentBounds(view_->layer()->bounds());
  }

  raw_ptr<views::View> view_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
};

////////////////////////////////////////////////////////////////////////////////
// SystemWindowDecoration:

// An extension of `SystemDecoration` for an `aura::Window`. The decoration
// layer is added to the window's layer and adjusts its content bounds with the
// window's bounds. Do not manually set the content bounds.
class SystemWindowDecoration : public SystemDecoration,
                               public aura::WindowObserver {
 public:
  explicit SystemWindowDecoration(aura::Window* window) : window_(window) {
    auto* window_layer = window_->layer();
    auto* decoration_layer = GetLayer();
    window_layer->Add(decoration_layer);
    window_layer->StackAtBottom(decoration_layer);
    SetContentBounds(window_layer->bounds());

    window_observation_.Observe(window_);

    if (window_->GetRootWindow()) {
      ObserveColorProviderSource(
          RootWindowController::ForWindow(window_)->color_provider_source());
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
    window_ = nullptr;
  }
  void OnWindowAddedToRootWindow(aura::Window* window) override {
    ObserveColorProviderSource(
        RootWindowController::ForWindow(window)->color_provider_source());
  }

 protected:
  // SystemDecoration:
  void OnHighlightBorderChanged() override {
    if (!window_) {
      return;
    }
    if (HasHighlightBorder()) {
      window_->layer()->StackAtTop(GetLayer());
    } else {
      window_->layer()->StackAtBottom(GetLayer());
    }
  }

 private:
  raw_ptr<aura::Window> window_ = nullptr;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

void ConfigureDecoration(SystemDecoration* decoration,
                         SystemDecoration::ShadowType shadow_type,
                         SystemDecoration::HighlightBorderMode border_mode) {
  decoration->SetShadow(shadow_type);
  decoration->SetHighlightBorder(border_mode);
}

}  // namespace

SystemDecoration::SystemDecoration()
    : decoration_(ui::Decoration::CreateShadowWithHighlightBorder()),
      source_(decoration_
                  ->GetSourceAs<ui::decoration::ShadowWithHighlightBorder>()) {}

SystemDecoration::~SystemDecoration() = default;

// static
std::unique_ptr<SystemDecoration> SystemDecoration::Create(
    ShadowType shadow_type,
    HighlightBorderMode border_mode) {
  auto decoration = base::WrapUnique(new SystemDecoration());
  ConfigureDecoration(decoration.get(), shadow_type, border_mode);
  return decoration;
}

// static
std::unique_ptr<SystemDecoration> SystemDecoration::CreateForView(
    views::View* view,
    ShadowType shadow_type,
    HighlightBorderMode border_mode) {
  DCHECK(view);
  auto decoration = base::WrapUnique(new SystemViewDecoration(view));
  ConfigureDecoration(decoration.get(), shadow_type, border_mode);
  return decoration;
}

// static
std::unique_ptr<SystemDecoration> SystemDecoration::CreateForWindow(
    aura::Window* window,
    ShadowType shadow_type,
    HighlightBorderMode border_mode) {
  DCHECK(window);
  auto decoration = base::WrapUnique(new SystemWindowDecoration(window));
  ConfigureDecoration(decoration.get(), shadow_type, border_mode);
  return decoration;
}

// static
int SystemDecoration::GetElevationFromType(ShadowType type) {
  switch (type) {
    case ShadowType::kNone:
      return 0;
    case ShadowType::kElevation4:
      return 4;
    case ShadowType::kElevation12:
      return 12;
    case ShadowType::kElevation24:
      return 24;
  }
}

void SystemDecoration::SetShadow(ShadowType type) {
  if (type == ShadowType::kNone) {
    if (!HasShadow()) {
      return;
    }
    source_->SetShadow(nullptr);
    UpdateHighlightBorder();
    return;
  }

  const int elevation = GetElevationFromType(type);
  if (auto* existing_shadow = source_->shadow()) {
    existing_shadow->SetElevation(elevation);
    return;
  }
  const ui::ColorProvider* color_provider =
      GetColorProviderSource() ? GetColorProviderSource()->GetColorProvider()
                               : nullptr;
  source_->SetShadow(std::make_unique<ui::decoration::Shadow>(
      elevation, ui::decoration::Shadow::Style::kChromeOSSystemUI,
      color_provider
          ? StyleUtil::CreateShadowElevationToColorsMap(color_provider)
          : ui::decoration::Shadow::ElevationToColorsMap()));
  UpdateHighlightBorder();
}

bool SystemDecoration::HasShadow() const {
  return source_->shadow() != nullptr;
}

void SystemDecoration::SetHighlightBorder(HighlightBorderMode mode) {
  switch (mode) {
    case HighlightBorderMode::kNone:
      if (!has_highlight_border_) {
        return;
      }
      has_highlight_border_ = false;
      source_->SetHighlightBorder(nullptr);
      OnHighlightBorderChanged();
      return;
    case HighlightBorderMode::kHighlightBorder:
      if (has_highlight_border_) {
        return;
      }
      has_highlight_border_ = true;
      OnHighlightBorderChanged();
      UpdateHighlightBorder();
      return;
  }
}

bool SystemDecoration::HasHighlightBorder() const {
  return has_highlight_border_;
}

void SystemDecoration::SetContentBounds(const gfx::Rect& bounds) {
  decoration_->SetContentBounds(bounds);
}

void SystemDecoration::SetRoundedCornerRadius(int corner_radius) {
  SetRoundedCorners(gfx::RoundedCornersF(corner_radius));
}

void SystemDecoration::SetRoundedCorners(
    const gfx::RoundedCornersF& rounded_corners) {
  decoration_->SetRoundedCorners(rounded_corners);
}

const gfx::Rect& SystemDecoration::GetContentBounds() const {
  return decoration_->content_bounds();
}

ui::Layer* SystemDecoration::GetLayer() {
  return decoration_->layer();
}

void SystemDecoration::ObserveColorProviderSource(
    ui::ColorProviderSource* color_provider_source) {
  Observe(color_provider_source);
}

void SystemDecoration::OnColorProviderChanged() {
  if (auto* color_provider_source = GetColorProviderSource()) {
    UpdateShadowColors(color_provider_source->GetColorProvider());
  }
  UpdateHighlightBorder();
}

const gfx::ShadowValues SystemDecoration::GetShadowValuesForTesting() const {
  const auto* shadow = source_->shadow();
  if (!shadow) {
    return gfx::ShadowValues();
  }
  return shadow
      ->GetSpec(decoration_->content_bounds(), decoration_->rounded_corners())
      .value_or(gfx::ShadowValues());
}

std::optional<ui::decoration::HighlightBorderValue>
SystemDecoration::GetHighlightBorderValueForTesting() const {
  const auto* border = source_->highlight_border();
  if (!border) {
    return std::nullopt;
  }
  return border->GetSpec(decoration_->content_bounds(),
                         decoration_->rounded_corners());
}

void SystemDecoration::ApplyHighlightBorder(SkColor highlight_color,
                                            SkColor border_color) {
  if (auto* existing_border = source_->highlight_border()) {
    existing_border->SetColors(highlight_color, border_color);
    return;
  }
  source_->SetHighlightBorder(std::make_unique<ui::decoration::HighlightBorder>(
      highlight_color, border_color, views::kHighlightBorderThickness));
}

void SystemDecoration::UpdateShadowColors(
    const ui::ColorProvider* color_provider) {
  if (auto* shadow = source_->shadow()) {
    shadow->SetColorMap(
        StyleUtil::CreateShadowElevationToColorsMap(color_provider));
  }
}

void SystemDecoration::UpdateHighlightBorder() {
  if (!has_highlight_border_ || !source_) {
    return;
  }
  auto* color_provider_source = GetColorProviderSource();
  if (!color_provider_source) {
    return;
  }
  const auto border_type =
      HasShadow() ? views::HighlightBorder::Type::kHighlightBorderOnShadow
                  : views::HighlightBorder::Type::kHighlightBorderNoShadow;
  const ui::ColorProvider* color_provider =
      color_provider_source->GetColorProvider();
  ApplyHighlightBorder(
      color_provider->GetColor(GetHighlightColorId(border_type)),
      color_provider->GetColor(GetBorderColorId(border_type)));
}

}  // namespace ash
