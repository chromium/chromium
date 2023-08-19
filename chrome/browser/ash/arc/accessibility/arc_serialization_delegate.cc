// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/arc/accessibility/arc_serialization_delegate.h"

#include "chrome/browser/ash/arc/accessibility/geometry_util.h"
#include "components/exo/shell_surface.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc {
void ArcSerializationDelegate::PopulateBounds(
    const ax::android::AccessibilityInfoDataWrapper& node,
    ui::AXNodeData& out_data) const {
  aura::Window* window = tree_source_->window();
  ax::android::AccessibilityInfoDataWrapper* root = tree_source_->GetRoot();
  gfx::Rect info_data_bounds = node.GetBounds();
  gfx::RectF& out_bounds_px = out_data.relative_bounds.bounds;

  if (window && root && exo::WMHelper::HasInstance()) {
    if (tree_source_->is_notification() ||
        tree_source_->is_input_method_window() ||
        root->GetId() != node.GetId()) {
      // By default, populate the bounds relative to the tree root.
      const gfx::Rect& root_bounds = root->GetBounds();
      info_data_bounds.Offset(-1 * root_bounds.x(), -1 * root_bounds.y());

      out_bounds_px = ScaleAndroidPxToChromePx(info_data_bounds, window);
      out_data.relative_bounds.offset_container_id = root->GetId();
    } else {
      // For the root node of application tree, populate the bounds to be
      // relative to its container View.
      views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
      CHECK(widget);
      CHECK(widget->widget_delegate());
      CHECK(widget->widget_delegate()->GetContentsView());
      gfx::PointF root_origin = gfx::PointF(widget->widget_delegate()
                                                ->GetContentsView()
                                                ->GetBoundsInScreen()
                                                .origin());

      // Android sends bounds in display coordinate.
      // Make window bounds relative to display so that we can compute the
      // actual offset of a11y window bounds from ash window.
      // TODO(hirokisato): Android pi sends different coordinate.
      const display::Display display =
          display::Screen::GetScreen()->GetDisplayNearestView(window);
      root_origin.Offset(-display.bounds().x(), -display.bounds().y());

      // Adjust the origin because a maximized window has an offset in
      // Android.
      root_origin.Offset(0, -1 * GetChromeWindowHeightOffsetInDip(window));

      // Scale to Chrome pixels.
      root_origin.Scale(
          window->GetToplevelWindow()->layer()->device_scale_factor());

      out_bounds_px = ScaleAndroidPxToChromePx(info_data_bounds, window);
      out_bounds_px.Offset(-1 * root_origin.x(), -1 * root_origin.y());
    }
  } else {
    // We cannot compute global bounds, so use the raw bounds.
    out_bounds_px.SetRect(info_data_bounds.x(), info_data_bounds.y(),
                          info_data_bounds.width(), info_data_bounds.height());
  }
}
}  // namespace arc
