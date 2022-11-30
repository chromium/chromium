// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_

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
  DCHECK(exo::WMHelper::HasInstance());
  DCHECK(window);

  const float chrome_dsf =
      window->GetToplevelWindow()->layer()->device_scale_factor();
  const float android_dsf =
      exo::WMHelper::GetInstance()->GetDeviceScaleFactorForWindow(window);
  if (chrome_dsf == android_dsf)
    return gfx::RectF(android_bounds);

  gfx::RectF chrome_bounds(android_bounds);
  chrome_bounds.Scale(chrome_dsf / android_dsf);
  return chrome_bounds;
}

int GetChromeWindowHeightOffsetInDip(aura::Window* window) {
  // On Android side, content is rendered without considering height of
  // caption bar when it's maximized, e.g. Content is rendered at y:0 instead of
  // y:32 where 32 is height of caption bar.
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (!widget->IsMaximized())
    return 0;

  return widget->non_client_view()->frame_view()->GetBoundsForClientView().y();
}
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_
