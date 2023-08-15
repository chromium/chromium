// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/geometry_util.h"

#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/widget/widget.h"

namespace arc {
gfx::RectF ScaleAndroidPxToChromePx(const gfx::Rect& android_bounds,
                                    aura::Window* window) {
  CHECK(exo::WMHelper::HasInstance());
  CHECK(window);

  if (!exo::WMHelper::GetInstance()->use_default_scale_cancellation()) {
    return gfx::RectF(android_bounds);
  }

  const aura::Window* toplevel_window = window->GetToplevelWindow();
  CHECK(toplevel_window);
  const ui::Layer* layer = toplevel_window->layer();
  CHECK(layer);
  const float chrome_dsf = layer->device_scale_factor();
  const float android_dsf =
      exo::WMHelper::GetInstance()->GetDeviceScaleFactorForWindow(window);
  if (chrome_dsf == android_dsf) {
    return gfx::RectF(android_bounds);
  }

  gfx::RectF chrome_bounds(android_bounds);
  chrome_bounds.Scale(chrome_dsf / android_dsf);
  return chrome_bounds;
}

int GetChromeWindowHeightOffsetInDip(aura::Window* window) {
  // On Android side, content is rendered without considering height of
  // caption bar when it's maximized, e.g. Content is rendered at y:0 instead of
  // y:32 where 32 is height of caption bar.
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (!widget->IsMaximized()) {
    return 0;
  }

  return widget->non_client_view()->frame_view()->GetBoundsForClientView().y();
}
}  // namespace arc
