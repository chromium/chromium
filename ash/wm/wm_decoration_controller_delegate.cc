// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/wm_decoration_controller_delegate.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/decoration_controller.h"
#include "ui/wm/core/shadow_types.h"

namespace {

//------------------------------------------------------------------------------
// DecorationColorizer:
// Observes the given color provider source and updates the shadow and highlight
// border colors with the colors from the color provider.
class DecorationColorizer : public ui::ColorProviderSourceObserver {
 public:
  DecorationColorizer(aura::Window* window,
                      ui::ColorProviderSource* color_provider_source)
      : window_(window) {
    Observe(color_provider_source);
  }

  DecorationColorizer(const DecorationColorizer&) = delete;
  DecorationColorizer& operator=(const DecorationColorizer&) = delete;
  ~DecorationColorizer() override = default;

  // ui::ColorProviderSourceObserver:
  void OnColorProviderChanged() override {
    // This function will also be called when color provider source is
    // destroying. We should guarantee the observed color provider source
    // exists.
    if (auto* color_provider_source = GetColorProviderSource()) {
      const auto* color_provider = color_provider_source->GetColorProvider();
      if (auto* shadow =
              wm::DecorationController::GetShadowForWindow(window_.get())) {
        shadow->SetColorMap(
            wm::DecorationController::GenerateShadowColorsMap(color_provider));
      }
      if (auto* highlight_border =
              wm::DecorationController::GetHighlightBorderForWindow(
                  window_.get())) {
        const auto [highlight_color, border_color] =
            wm::DecorationController::GetHighlightBorderColors(color_provider);
        highlight_border->SetColors(highlight_color, border_color);
      }
    }
  }

 private:
  const raw_ptr<aura::Window> window_;
};

}  // namespace

DEFINE_UI_CLASS_PROPERTY_TYPE(DecorationColorizer*)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(DecorationColorizer, kDecorationColorizerKey)
namespace ash {

//------------------------------------------------------------------------------
// WmDecorationControllerDelegate:
WmDecorationControllerDelegate::WmDecorationControllerDelegate() = default;

WmDecorationControllerDelegate::~WmDecorationControllerDelegate() = default;

bool WmDecorationControllerDelegate::ShouldObserveWindow(
    const aura::Window* window) {
  // On ChromeOS, Containers (which has no delegate with unknown type) and
  // embedded windows do not need Shadow.  Other windows will be checked when a
  // window is added to tree, or its property changes.
  return ((window->delegate() ||
           window->GetType() != aura::client::WINDOW_TYPE_UNKNOWN) &&
          window->GetType() != aura::client::WINDOW_TYPE_CONTROL);
}

bool WmDecorationControllerDelegate::ShouldShowShadowForWindow(
    const aura::Window* window) {
  // Hide the shadow if it is one of the splitscreen snapped windows.
  if (window->GetRootWindow() && RootWindowController::ForWindow(window)) {
    SplitViewController* split_view_controller =
        SplitViewController::Get(window);
    if (split_view_controller &&
        split_view_controller->IsWindowInSplitView(window)) {
      return false;
    }
  }

  // Hide the shadow while we are in overview mode.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller && overview_controller->InOverviewSession()) {
    OverviewSession* overview_session = overview_controller->overview_session();
    // InOverviewSession() being true implies |overview_session| exists.
    DCHECK(overview_session);
    // Windows in overview that are not moving out of the active desk should not
    // have shadows.
    auto* overview_item = overview_session->GetOverviewItemForWindow(window);
    if (overview_item && !overview_item->is_moving_to_another_desk()) {
      return false;
    }
  }

  // The shadow state will be updated when the window is added to a parent.
  if (!window->parent()) {
    return false;
  }

  // Show the shadow if it's currently being dragged no matter of the window's
  // show state.
  auto* window_state = WindowState::Get(window);
  if (window_state && window_state->is_dragged()) {
    return ::wm::GetShadowElevationConvertDefault(window) > 0;
  }

  // Hide the shadow if it's not being dragged and it's a maximized/fullscreen
  // window.
  ui::mojom::WindowShowState show_state =
      window->GetProperty(aura::client::kShowStateKey);
  if (show_state == ui::mojom::WindowShowState::kFullscreen ||
      show_state == ui::mojom::WindowShowState::kMaximized) {
    return false;
  }

  return ::wm::GetShadowElevationConvertDefault(window) > 0;
}

bool WmDecorationControllerDelegate::ShouldShowHighlightBorderForWindow(
    const aura::Window* window) {
  if (!window->parent()) {
    return false;
  }

  if (!window->GetProperty(::wm::kWindowHasHighlightBorderKey)) {
    return false;
  }

  // Hide the highlight border while we are in overview mode.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller && overview_controller->InOverviewSession()) {
    OverviewSession* overview_session = overview_controller->overview_session();
    DCHECK(overview_session);
    auto* overview_item = overview_session->GetOverviewItemForWindow(window);
    if (overview_item && !overview_item->is_moving_to_another_desk()) {
      return false;
    }
  }

  ui::mojom::WindowShowState show_state =
      window->GetProperty(aura::client::kShowStateKey);
  return show_state != ui::mojom::WindowShowState::kFullscreen &&
         show_state != ui::mojom::WindowShowState::kMaximized;
}

bool WmDecorationControllerDelegate::
    ShouldUpdateDecorationOnWindowPropertyChange(const aura::Window* window,
                                                 const void* key,
                                                 intptr_t old) {
  if (key == chromeos::kWindowHasRoundedCornersKey) {
    return window->GetProperty(chromeos::kWindowHasRoundedCornersKey) !=
           static_cast<bool>(old);
  }
  if (key == chromeos::kIsShowingInOverviewKey) {
    return window->GetProperty(chromeos::kIsShowingInOverviewKey) !=
           static_cast<bool>(old);
  }
  return false;
}

void WmDecorationControllerDelegate::ApplyColorThemeToWindowDecoration(
    aura::Window* window) {
  if (auto* color_provider_source =
          views::Widget::GetWidgetForNativeWindow(window)) {
    window->SetProperty(
        kDecorationColorizerKey,
        std::make_unique<DecorationColorizer>(window, color_provider_source));
  }
}

bool WmDecorationControllerDelegate::ShouldRoundDecorationForWindow(
    const aura::Window* window) {
  auto* window_state = WindowState::Get(window);
  return window_state ? window_state->ShouldWindowHaveRoundedCorners() : true;
}

}  // namespace ash
