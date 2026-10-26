// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_decoration.h"

#include <memory>
#include <optional>

#include "ash/root_window_controller.h"
#include "ash/style/style_util.h"
#include "base/check.h"
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
#include "ui/views/view_decoration.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

ui::decoration::Shadow* GetShadow(ui::Decoration* decoration) {
  return decoration
             ? decoration
                   ->GetSourceAs<ui::decoration::ShadowWithHighlightBorder>()
                   ->shadow()
             : nullptr;
}

const ui::decoration::Shadow* GetShadow(const ui::Decoration* decoration) {
  return decoration
             ? decoration
                   ->GetSourceAs<ui::decoration::ShadowWithHighlightBorder>()
                   ->shadow()
             : nullptr;
}

const ui::decoration::HighlightBorder* GetHighlightBorder(
    const ui::Decoration* decoration) {
  return decoration
             ? decoration
                   ->GetSourceAs<ui::decoration::ShadowWithHighlightBorder>()
                   ->highlight_border()
             : nullptr;
}

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
// SystemDecorationImpl:

// An implementation of `SystemDecoration`. It directly owns a
// `ShadowWithHighlightBorder` `ui::Decoration`.
class SystemDecorationImpl : public SystemDecoration {
 public:
  SystemDecorationImpl()
      : decoration_(ui::Decoration::CreateShadowWithHighlightBorder()),
        source_(
            decoration_
                ->GetSourceAs<ui::decoration::ShadowWithHighlightBorder>()) {}

  SystemDecorationImpl(const SystemDecorationImpl&) = delete;
  SystemDecorationImpl& operator=(const SystemDecorationImpl&) = delete;

  ~SystemDecorationImpl() override = default;

 protected:
  // SystemDecoration:
  ui::Decoration* decoration() override { return decoration_.get(); }
  const ui::Decoration* decoration() const override {
    return decoration_.get();
  }

  void ApplyHighlightBorder(SkColor highlight_color,
                            SkColor border_color) override {
    if (auto* existing_border = source_->highlight_border()) {
      existing_border->SetColors(highlight_color, border_color);
      return;
    }
    source_->SetHighlightBorder(
        std::make_unique<ui::decoration::HighlightBorder>(
            highlight_color, border_color, views::kHighlightBorderThickness));
  }

  void ApplyClearHighlightBorder() override {
    source_->SetHighlightBorder(nullptr);
  }

 private:
  std::unique_ptr<ui::Decoration> decoration_;
  raw_ptr<ui::decoration::ShadowWithHighlightBorder> source_;
};

////////////////////////////////////////////////////////////////////////////////
// SystemViewDecoration:

// An implementation of `SystemDecoration` based on `views::ViewDecoration`.
// The decoration layer is added to the view's region layers and adjusts its
// content bounds with the view's bounds. Do not manually set the content
// bounds.
class SystemViewDecoration : public SystemDecoration,
                             public views::ViewObserver {
 public:
  explicit SystemViewDecoration(views::View* view) : view_decoration_(view) {
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

  void ApplyHighlightBorder(SkColor highlight_color,
                            SkColor border_color) override {
    view_decoration_.SetHighlightBorder(highlight_color, border_color,
                                        views::kHighlightBorderThickness);
  }

  void ApplyClearHighlightBorder() override {
    view_decoration_.ClearHighlightBorder();
  }

  views::ViewDecoration view_decoration_;
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
};

////////////////////////////////////////////////////////////////////////////////
// SystemWindowDecoration:

// An extension of `SystemDecorationImpl`. The decoration layer is added to a
// window's layer and adjusts its content bounds with the window's bounds. Do
// not manually set the content bounds.
class SystemWindowDecoration : public SystemDecorationImpl,
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
  // SystemDecorationImpl:
  void ApplyHighlightBorder(SkColor highlight_color,
                            SkColor border_color) override {
    const bool had_border = GetHighlightBorder(decoration()) != nullptr;
    SystemDecorationImpl::ApplyHighlightBorder(highlight_color, border_color);
    if (!had_border && window_) {
      window_->layer()->StackAtTop(GetLayer());
    }
  }

  void ApplyClearHighlightBorder() override {
    const bool had_border = GetHighlightBorder(decoration()) != nullptr;
    SystemDecorationImpl::ApplyClearHighlightBorder();
    if (had_border && window_) {
      window_->layer()->StackAtBottom(GetLayer());
    }
  }

 private:
  raw_ptr<aura::Window> window_ = nullptr;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

void ConfigureDecoration(
    SystemDecoration* decoration,
    std::optional<SystemDecoration::ShadowType> shadow_type,
    SystemDecoration::HighlightBorderMode border_mode) {
  if (shadow_type.has_value()) {
    decoration->SetShadow(*shadow_type);
  }
  if (border_mode == SystemDecoration::HighlightBorderMode::kHighlightBorder) {
    decoration->SetHighlightBorder();
  }
}

}  // namespace

SystemDecoration::~SystemDecoration() = default;

// static
std::unique_ptr<SystemDecoration> SystemDecoration::Create(
    std::optional<ShadowType> shadow_type,
    HighlightBorderMode border_mode) {
  auto decoration = std::make_unique<SystemDecorationImpl>();
  ConfigureDecoration(decoration.get(), shadow_type, border_mode);
  return decoration;
}

// static
std::unique_ptr<SystemDecoration> SystemDecoration::CreateForView(
    views::View* view,
    std::optional<ShadowType> shadow_type,
    HighlightBorderMode border_mode) {
  DCHECK(view);
  auto decoration = std::make_unique<SystemViewDecoration>(view);
  ConfigureDecoration(decoration.get(), shadow_type, border_mode);
  return decoration;
}

// static
std::unique_ptr<SystemDecoration> SystemDecoration::CreateForWindow(
    aura::Window* window,
    std::optional<ShadowType> shadow_type,
    HighlightBorderMode border_mode) {
  DCHECK(window);
  auto decoration = std::make_unique<SystemWindowDecoration>(window);
  ConfigureDecoration(decoration.get(), shadow_type, border_mode);
  return decoration;
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

void SystemDecoration::SetShadow(ShadowType type) {
  if (!decoration()) {
    return;
  }
  auto* source =
      decoration()->GetSourceAs<ui::decoration::ShadowWithHighlightBorder>();
  const int elevation = GetElevationFromType(type);
  if (auto* existing_shadow = source->shadow()) {
    existing_shadow->SetElevation(elevation);
    return;
  }
  const ui::ColorProvider* color_provider =
      GetColorProviderSource() ? GetColorProviderSource()->GetColorProvider()
                               : nullptr;
  source->SetShadow(std::make_unique<ui::decoration::Shadow>(
      elevation, ui::decoration::Shadow::Style::kChromeOSSystemUI,
      color_provider
          ? StyleUtil::CreateShadowElevationToColorsMap(color_provider)
          : ui::decoration::Shadow::ElevationToColorsMap()));
  UpdateHighlightBorder();
}

void SystemDecoration::ClearShadow() {
  if (decoration()) {
    decoration()
        ->GetSourceAs<ui::decoration::ShadowWithHighlightBorder>()
        ->SetShadow(nullptr);
    UpdateHighlightBorder();
  }
}

bool SystemDecoration::HasShadow() const {
  return GetShadow(decoration()) != nullptr;
}

void SystemDecoration::SetHighlightBorder() {
  has_highlight_border_ = true;
  UpdateHighlightBorder();
}

void SystemDecoration::ClearHighlightBorder() {
  if (!has_highlight_border_) {
    return;
  }
  has_highlight_border_ = false;
  ApplyClearHighlightBorder();
}

bool SystemDecoration::HasHighlightBorder() const {
  return has_highlight_border_;
}

void SystemDecoration::SetContentBounds(const gfx::Rect& bounds) {
  if (decoration()) {
    decoration()->SetContentBounds(bounds);
  }
}

void SystemDecoration::SetRoundedCornerRadius(int corner_radius) {
  SetRoundedCorners(gfx::RoundedCornersF(corner_radius));
}

void SystemDecoration::SetRoundedCorners(
    const gfx::RoundedCornersF& rounded_corners) {
  if (decoration()) {
    decoration()->SetRoundedCorners(rounded_corners);
  }
}

const gfx::Rect& SystemDecoration::GetContentBounds() {
  return decoration()->content_bounds();
}

ui::Layer* SystemDecoration::GetLayer() {
  return decoration() ? decoration()->layer() : nullptr;
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
  const auto* shadow = GetShadow(decoration());
  if (!shadow) {
    return gfx::ShadowValues();
  }
  return shadow
      ->GetSpec(decoration()->content_bounds(), decoration()->rounded_corners())
      .value_or(gfx::ShadowValues());
}

std::optional<ui::decoration::HighlightBorderValue>
SystemDecoration::GetHighlightBorderValueForTesting() const {
  const auto* border = GetHighlightBorder(decoration());
  if (!border) {
    return std::nullopt;
  }
  return border->GetSpec(decoration()->content_bounds(),
                         decoration()->rounded_corners());
}

void SystemDecoration::UpdateShadowColors(
    const ui::ColorProvider* color_provider) {
  if (auto* shadow = GetShadow(decoration())) {
    shadow->SetColorMap(
        StyleUtil::CreateShadowElevationToColorsMap(color_provider));
  }
}

void SystemDecoration::UpdateHighlightBorder() {
  if (!has_highlight_border_ || !decoration()) {
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
