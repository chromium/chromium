// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/annotator/annotations_overlay_controller.h"

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/annotator/annotation_tray.h"
#include "ash/annotator/annotator_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/public/cpp/annotator/annotations_overlay_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_properties.h"

namespace ash {

namespace {

// When annotating on a non-root window, the overlay is added as a direct
// child of the window, and stacked on top of all children. This is so that
// the overlay contents show up above everything else.
//
//   + window
//       |
//       + (Some other child windows hosting contents of the window)
//       |
//       + Annotations overlay widget
//
// (Note that bottom-most child are the top-most child in terms of z-order).
//
// However, when annotating on the root window, the overlay is added as a child
// of the menu container.
// The menu container is high enough in terms of z-order, making the overlay on
// top of most things. However, it's also the same container used by the
// projector bar (which we want to be on top of the overlay, since it has the
// button to toggle the overlay off, and we don't want the overlay to block
// events going to that button). Therefore, the overlay is stacked at the bottom
// of the menu container's children. See UpdateWidgetStacking() below.
//
//   + Menu container
//     |
//     + Annotations overlay widget
//     |
//     + Projector bar widget
//
// TODO(crbug.com/40199022): Revise this parenting and z-ordering once
// the deprecated Projector toolbar is removed and replaced by the shelf-pod
// based new tools.
aura::Window* GetWidgetParent(aura::Window* window) {
  return window->IsRootWindow()
             ? window->GetChildById(kShellWindowId_MenuContainer)
             : window;
}

// Given the `bounds_in_parent` of the overlay widget, returns the bounds in the
// correct coordinate system depending on whether the `overlay_window_parent`
// uses screen coordinates or not.
gfx::Rect MaybeAdjustOverlayBounds(const gfx::Rect& bounds_in_parent,
                                   aura::Window* overlay_window_parent) {
  DCHECK(overlay_window_parent);
  if (!overlay_window_parent->GetProperty(wm::kUsesScreenCoordinatesKey))
    return bounds_in_parent;
  gfx::Rect bounds_in_screen = bounds_in_parent;
  wm::ConvertRectToScreen(overlay_window_parent, &bounds_in_screen);
  return bounds_in_screen;
}

// Defines a window targeter that will be installed on the overlay widget's
// window so that we can allow located events over the projector shelf pod or
// its associated bubble widget to go through and not be consumed by the
// overlay. This enables the user to interact with the annotation tools while
// annotating.
class OverlayTargeter : public aura::WindowTargeter {
 public:
  explicit OverlayTargeter(aura::Window* overlay_window)
      : overlay_window_(overlay_window) {}
  OverlayTargeter(const OverlayTargeter&) = delete;
  OverlayTargeter& operator=(const OverlayTargeter&) = delete;
  ~OverlayTargeter() override = default;

  // aura::WindowTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    if (event->IsLocatedEvent()) {
      auto* root_window = overlay_window_->GetRootWindow();
      auto* status_area_widget =
          RootWindowController::ForWindow(root_window)->GetStatusAreaWidget();
      StopRecordingButtonTray* stop_recording_button =
          status_area_widget->stop_recording_button_tray();
      auto screen_location = event->AsLocatedEvent()->root_location();
      wm::ConvertPointToScreen(root_window, &screen_location);

      Shelf* shelf = RootWindowController::ForWindow(root_window)->shelf();
      // To be able to bring the auto-hidden shelf back even while annotation is
      // active, we expose a slim 1dp region at the edge of the screen in which
      // the shelf is aligned. Events in that region will not be consumed so
      // that they can be used to show the auto-hidden shelf.
      if (!shelf->IsVisible()) {
        gfx::Rect root_window_bounds_in_screen =
            root_window->GetBoundsInScreen();
        const int display_width = root_window_bounds_in_screen.width();
        const int display_height = root_window_bounds_in_screen.height();
        const gfx::Rect shelf_activation_bounds =
            shelf->SelectValueForShelfAlignment(
                gfx::Rect(0, display_height - 1, display_width, 1),
                gfx::Rect(0, 0, 1, display_height),
                gfx::Rect(display_width - 1, 0, 1, display_height));

        if (shelf_activation_bounds.Contains(screen_location))
          return nullptr;
      }

      // To be able to end video recording even while annotation is active,
      // let events over the stop recording button to go through.
      if (stop_recording_button && stop_recording_button->visible_preferred() &&
          stop_recording_button->GetBoundsInScreen().Contains(
              screen_location)) {
        return nullptr;
      }

      AnnotationTray* annotations = status_area_widget->annotation_tray();
      if (annotations && annotations->visible_preferred()) {
        // Let events over the projector shelf pod to go through.
        if (annotations->GetBoundsInScreen().Contains(screen_location))
          return nullptr;

        // Let events over the projector bubble widget (if shown) to go through.
        views::Widget* bubble_widget = annotations->GetBubbleWidget();
        if (bubble_widget && bubble_widget->IsVisible() &&
            bubble_widget->GetWindowBoundsInScreen().Contains(
                screen_location)) {
          return nullptr;
        }

        // Ensure that the annotator bubble is closed when a press event is
        // triggered.
        if (event->IsLocatedEvent() &&
            (event->type() == ui::EventType::kMousePressed ||
             event->type() == ui::EventType::kTouchPressed)) {
          annotations->ClickedOutsideBubble(*event->AsLocatedEvent());
        }
      }
    }

    return aura::WindowTargeter::FindTargetForEvent(root, event);
  }

 private:
  const raw_ptr<aura::Window> overlay_window_;
};

}  // namespace

AnnotationsOverlayController::AnnotationsOverlayController(
    aura::Window* window,
    std::optional<gfx::Rect> partial_region_bounds)
    : window_(window), partial_region_bounds_(partial_region_bounds) {
  DCHECK(window_);
  const gfx::Rect initial_bounds_in_parent = GetOverlayWidgetBounds();
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "AnnotationsOverlayWidget";
  params.child = true;
  params.parent = GetWidgetParent(window_);

  // The overlay hosts transparent contents so actual contents of the window
  // shows up underneath.
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.bounds =
      MaybeAdjustOverlayBounds(initial_bounds_in_parent, params.parent);
  // The overlay window does not receive any events until it's shown and
  // enabled. See |Start()| below.
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = false;
  overlay_widget_->Init(std::move(params));
  annotations_overlay_view_ = overlay_widget_->SetContentsView(
      Shell::Get()->annotator_controller()->CreateAnnotationsOverlayView());
  auto* overlay_window = overlay_widget_->GetNativeWindow();
  overlay_window->SetEventTargeter(
      std::make_unique<OverlayTargeter>(overlay_window));
  UpdateWidgetStacking();
  display_observation_.Observe(display::Screen::GetScreen());
  window_observation_.Observe(window_);
}

AnnotationsOverlayController::~AnnotationsOverlayController() {
  Reset();
}

void AnnotationsOverlayController::Toggle() {
  is_enabled_ = !is_enabled_;
  if (is_enabled_)
    Start();
  else
    Stop();
}

aura::Window* AnnotationsOverlayController::GetOverlayNativeWindow() {
  return overlay_widget_->GetNativeWindow();
}

void AnnotationsOverlayController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  SetBounds(GetOverlayWidgetBounds());
}

void AnnotationsOverlayController::OnWindowDestroying(aura::Window* window) {
  Reset();
}

void AnnotationsOverlayController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  // TODO(b/342104047): Check if DISPLAY_METRIC_BOUNDS and
  // DISPLAY_METRIC_ROTATION need to be considered here as well.
  if (metrics & DISPLAY_METRIC_WORK_AREA) {
    SetBounds(GetOverlayWidgetBounds());
  }
}

void AnnotationsOverlayController::Start() {
  DCHECK(is_enabled_);

  overlay_widget_->GetNativeWindow()->SetEventTargetingPolicy(
      aura::EventTargetingPolicy::kTargetAndDescendants);
  overlay_widget_->Show();
}

void AnnotationsOverlayController::Stop() {
  DCHECK(!is_enabled_);

  overlay_widget_->GetNativeWindow()->SetEventTargetingPolicy(
      aura::EventTargetingPolicy::kNone);
  overlay_widget_->Hide();
}

void AnnotationsOverlayController::UpdateWidgetStacking() {
  auto* overlay_window = overlay_widget_->GetNativeWindow();
  auto* parent = overlay_window->parent();
  DCHECK(parent);

  // See more info in the docs of GetWidgetParent() above.
  if (parent->GetId() == kShellWindowId_MenuContainer)
    parent->StackChildAtBottom(overlay_window);
  else
    parent->StackChildAtTop(overlay_window);
}

void AnnotationsOverlayController::SetBounds(
    const gfx::Rect& bounds_in_parent) {
  overlay_widget_->SetBounds(MaybeAdjustOverlayBounds(
      bounds_in_parent, overlay_widget_->GetNativeWindow()));
}

gfx::Rect AnnotationsOverlayController::GetOverlayWidgetBounds() const {
  gfx::Rect bounds =
      partial_region_bounds_.has_value()
          ? capture_mode_util::GetEffectivePartialRegionBounds(
                partial_region_bounds_.value(), window_->GetRootWindow())
          : gfx::Rect(window_->bounds().size());
  bounds.Subtract(
      Shell::Get()
          ->docked_magnifier_controller()
          ->GetTotalMagnifierBoundsForRoot(window_->GetRootWindow()));
  return bounds;
}

void AnnotationsOverlayController::Reset() {
  window_observation_.Reset();
  display_observation_.Reset();
  overlay_widget_.reset();
  annotations_overlay_view_ = nullptr;
  window_ = nullptr;
}

}  // namespace ash
