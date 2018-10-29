// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/immersive_gesture_handler_classic.h"

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_app_window_drag_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// How many pixels are reserved for gesture events to start dragging the app
// window from the top of the screen in tablet mode.
constexpr int kDragStartTopEdgeInset = 8;

// Check whether we should start gesture dragging app window according to the
// given ET_GETURE_SCROLL_BEGIN type |event|.
bool CanBeginGestureDrag(ui::GestureEvent* event) {
  if (event->details().scroll_y_hint() < 0)
    return false;

  const gfx::Point location_in_screen =
      event->target()->GetScreenLocation(*event);
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(static_cast<aura::Window*>(event->target()))
          .work_area();

  gfx::Rect hit_bounds_in_screen(work_area_bounds);
  hit_bounds_in_screen.set_height(kDragStartTopEdgeInset);
  if (hit_bounds_in_screen.Contains(location_in_screen))
    return true;

  // There may be a bezel sensor off screen logically above
  // |hit_bounds_in_screen|. Handles the ET_GESTURE_SCROLL_BEGIN event
  // triggerd in the bezel area too.
  return (location_in_screen.y() < hit_bounds_in_screen.y() &&
          location_in_screen.x() >= hit_bounds_in_screen.x() &&
          location_in_screen.x() < hit_bounds_in_screen.right());
}

}  // namespace

bool ImmersiveGestureHandlerClassic::CanDrag(ui::GestureEvent* event) {
  if (!base::FeatureList::IsEnabled(ash::features::kDragAppsInTabletMode))
    return false;

  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      static_cast<aura::Window*>(event->target()));
  if (!widget)
    return false;

  aura::Window* window = widget->GetNativeWindow();
  // Only process the event if its target has the same native window as
  // |immersive_fullscreen_controller_->widget()|.
  if (window != immersive_fullscreen_controller_->widget()->GetNativeWindow())
    return false;

  // Maximized, fullscreened and snapped windows in tablet mode are allowed to
  // be dragged.
  wm::WindowState* window_state = wm::GetWindowState(window);
  if (!window_state ||
      (!window_state->IsMaximized() && !window_state->IsFullscreen() &&
       !window_state->IsSnapped()) ||
      !Shell::Get()
           ->tablet_mode_controller()
           ->IsTabletModeWindowManagerEnabled()) {
    return false;
  }

  // Fullscreen browser windows are not draggable. Dragging from top should show
  // the frame.
  if (window_state->IsFullscreen() &&
      window->GetProperty(aura::client::kAppType) ==
          static_cast<int>(AppType::BROWSER)) {
    return false;
  }

  // Start to drag window until ET_GESTURE_SCROLL_BEGIN event happened.
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    if (!CanBeginGestureDrag(event))
      return false;
    tablet_mode_app_window_drag_controller_ =
        std::make_unique<TabletModeAppWindowDragController>();
    return true;
  }

  // Should not start dragging window until
  // |tablet_mode_app_window_drag_controller_| have been created.
  if (event->type() != ui::ET_GESTURE_SCROLL_BEGIN &&
      !tablet_mode_app_window_drag_controller_) {
    return false;
  }

  return true;
}

ImmersiveGestureHandlerClassic::ImmersiveGestureHandlerClassic(
    ImmersiveFullscreenController* controller)
    : immersive_fullscreen_controller_(controller) {
  Shell::Get()->AddPreTargetHandler(this);
}

ImmersiveGestureHandlerClassic::~ImmersiveGestureHandlerClassic() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void ImmersiveGestureHandlerClassic::OnGestureEvent(ui::GestureEvent* event) {
  if (CanDrag(event)) {
    DCHECK(tablet_mode_app_window_drag_controller_);
    if (tablet_mode_app_window_drag_controller_->DragWindowFromTop(event))
      event->SetHandled();
    return;
  }
  immersive_fullscreen_controller_->OnGestureEvent(event);
}

}  // namespace ash
