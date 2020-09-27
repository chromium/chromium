// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/frame_utils.h"

#include "ash/public/cpp/window_properties.h"
#include "chromeos/ui/chromeos_ui_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/hit_test_utils.h"
#include "ui/views/window/non_client_view.h"

namespace ash {

using WindowOpacity = views::Widget::InitParams::WindowOpacity;

int FrameBorderNonClientHitTest(views::NonClientFrameView* view,
                                const gfx::Point& point_in_widget) {
  gfx::Rect expanded_bounds = view->bounds();
  int outside_bounds = chromeos::kResizeOutsideBoundsSize;

  if (aura::Env::GetInstance()->is_touch_down())
    outside_bounds *= chromeos::kResizeOutsideBoundsScaleForTouch;
  expanded_bounds.Inset(-outside_bounds, -outside_bounds);

  if (!expanded_bounds.Contains(point_in_widget))
    return HTNOWHERE;

  // Check the frame first, as we allow a small area overlapping the contents
  // to be used for resize handles.
  views::Widget* widget = view->GetWidget();
  bool can_ever_resize = widget->widget_delegate()->CanResize();
  // Don't allow overlapping resize handles when the window is maximized or
  // fullscreen, as it can't be resized in those states.
  int resize_border = chromeos::kResizeInsideBoundsSize;
  if (widget->IsMaximized() || widget->IsFullscreen()) {
    resize_border = 0;
    can_ever_resize = false;
  }
  int frame_component = view->GetHTComponentForFrame(
      point_in_widget, resize_border, resize_border,
      chromeos::kResizeAreaCornerSize, chromeos::kResizeAreaCornerSize,
      can_ever_resize);
  if (frame_component != HTNOWHERE)
    return frame_component;

  int client_component =
      widget->client_view()->NonClientHitTest(point_in_widget);
  if (client_component != HTNOWHERE)
    return client_component;

  // Check if it intersects with children (frame caption button, back button,
  // etc.).
  int hit_test_component =
      views::GetHitTestComponent(widget->non_client_view(), point_in_widget);
  if (hit_test_component != HTNOWHERE)
    return hit_test_component;

  // Caption is a safe default.
  return HTCAPTION;
}

void ResolveInferredOpacity(views::Widget::InitParams* params) {
  DCHECK_EQ(params->opacity, WindowOpacity::kInferred);
  if (params->type == views::Widget::InitParams::TYPE_WINDOW &&
      params->layer_type == ui::LAYER_TEXTURED) {
    // A framed window may have a rounded corner which requires the
    // window to be transparent. WindowManager controls the actual
    // opaque-ness of the window depending on its window state.
    params->init_properties_container.SetProperty(
        ash::kWindowManagerManagesOpacityKey, true);
    params->opacity = WindowOpacity::kTranslucent;
  } else {
    params->opacity = WindowOpacity::kOpaque;
  }
}

bool ShouldUseRestoreFrame(const views::Widget* widget) {
  aura::Window* window = widget->GetNativeWindow();
  // This is true when dragging a maximized window in ash. During this phase,
  // the window should look as if it was restored, but keep its maximized state.
  if (window->GetProperty(kFrameRestoreLookKey))
    return true;

  // Maximized and fullscreen windows should use the maximized frame.
  if (widget->IsMaximized() || widget->IsFullscreen())
    return false;

  return true;
}

}  // namespace ash
