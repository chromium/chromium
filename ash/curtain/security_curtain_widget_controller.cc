// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/curtain/security_curtain_widget_controller.h"

#include "ash/curtain/input_event_filter.h"
#include "base/scoped_observation.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash::curtain {

namespace {

std::vector<std::unique_ptr<ui::Layer>> InitWidgetLayers(
    ui::Layer& root_layer) {
  // In rare cases the compositor might fail to allocate the textures.
  // To prevent the widget from being transparent in this case, we add a
  // solid color layer.
  auto solid_color_layer = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  solid_color_layer->SetColor(SK_ColorLTGRAY);
  root_layer.Add(solid_color_layer.get());

  auto textured_layer = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
  root_layer.Add(textured_layer.get());
  root_layer.StackAtTop(textured_layer.get());

  std::vector<std::unique_ptr<ui::Layer>> layers;
  layers.push_back(std::move(solid_color_layer));
  layers.push_back(std::move(textured_layer));
  return layers;
}

views::Widget::InitParams GetWidgetInitParams(aura::Window* parent) {
  views::Widget::InitParams result;
  result.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  result.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  result.name = "CurtainOverlayWidget";
  result.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  result.parent = parent;
  // The curtain screen should not consume any events, but instead the windows
  // below should continue to receive all mouse/keyboard/... events.
  result.accept_events = false;
  // No need to set `show_state` as the window bounds are managed by the parent.
  return result;
}

std::unique_ptr<views::View> CreateContentsView() {
  // Use a simple solid colored curtain for now.
  // TODO(b/243626980): Update once UX has decided on the content of the
  // curtain screen.
  auto result = std::make_unique<views::View>();
  result->SetLayoutManager(std::make_unique<views::FillLayout>());
  result->SetBackground(views::CreateSolidBackground(SK_ColorYELLOW));
  return result;
}

std::unique_ptr<views::Widget> CreateWidget(aura::Window* parent) {
  auto widget = std::make_unique<views::Widget>();
  widget->Init(GetWidgetInitParams(parent));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  widget->SetContentsView(CreateContentsView());
  return widget;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//  WidgetMaximizer
////////////////////////////////////////////////////////////////////////////////

// Helper class that ensures the curtain widget is always maximized,
// even when the display is resized.
class SecurityCurtainWidgetController::WidgetMaximizer
    : public aura::WindowObserver {
 public:
  explicit WidgetMaximizer(views::Widget* widget) : widget_(*widget) {
    // Observe resizes
    root_window_observation_.Observe(&root_window());

    // Set initial layer dimensions
    OnRootWindowResized();
  }
  WidgetMaximizer(const WidgetMaximizer&) = delete;
  WidgetMaximizer& operator=(const WidgetMaximizer&) = delete;
  ~WidgetMaximizer() override = default;

 private:
  // aura::WindowObserver implementation:
  void OnWindowBoundsChanged(aura::Window* root,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    OnRootWindowResized();
  }

  void OnRootWindowResized() {
    gfx::Rect new_bounds(root_window().layer()->bounds().size());
    widget_->SetBounds(new_bounds);
  }

  aura::Window& root_window() {
    auto* result = widget_->GetNativeWindow()->GetRootWindow();
    DCHECK(result);
    return *result;
  }

  // We should track when the root window is resized to ensure our widget
  // remains full screen.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      root_window_observation_{this};

  raw_ref<views::Widget> widget_;
};

////////////////////////////////////////////////////////////////////////////////
//  SecurityCurtainWidgetController
////////////////////////////////////////////////////////////////////////////////

SecurityCurtainWidgetController::SecurityCurtainWidgetController(
    SecurityCurtainWidgetController&&) = default;
SecurityCurtainWidgetController& SecurityCurtainWidgetController::operator=(
    SecurityCurtainWidgetController&&) = default;
SecurityCurtainWidgetController::~SecurityCurtainWidgetController() = default;

SecurityCurtainWidgetController::SecurityCurtainWidgetController(
    std::unique_ptr<views::Widget> widget,
    Layers layers,
    std::unique_ptr<InputEventFilter> input_event_filter)
    : widget_layers_(std::move(layers)),
      widget_(std::move(widget)),
      occlusion_tracker_exclude_(
          std::make_unique<aura::WindowOcclusionTracker::ScopedExclude>(
              widget_->GetNativeView())),
      widget_maximizer_(std::make_unique<WidgetMaximizer>(widget_.get())),
      input_event_filter_(std::move(input_event_filter)) {
  DCHECK(widget_);
  widget_->Show();
}

// static
SecurityCurtainWidgetController
SecurityCurtainWidgetController::CreateForRootWindow(aura::Window* root_window,
                                                     EventFilter event_filter) {
  auto widget = CreateWidget(root_window);
  auto layers = InitWidgetLayers(*widget->GetLayer());
  return SecurityCurtainWidgetController(
      std::move(widget), std::move(layers),
      std::make_unique<InputEventFilter>(root_window, event_filter));
}

const views::Widget& SecurityCurtainWidgetController::GetWidget() const {
  DCHECK(widget_);
  return *widget_;
}

views::Widget& SecurityCurtainWidgetController::GetWidget() {
  DCHECK(widget_);
  return *widget_;
}

}  // namespace ash::curtain
