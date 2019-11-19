// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/dip_px_util.h"

#include "ui/base/layout.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

// TODO(crbug.com/826982): plumb through enough information to use one of
// Screen::GetDisplayNearest{Window/View/Point}. That way in multi-monitor
// setups where one screen is hidpi and the other one isn't, we don't always do
// the wrong thing.

namespace {

float GetPrimaryDisplayScaleFactor() {
  display::Screen* screen = display::Screen::GetScreen();
  if (!screen) {
    return 1.0f;
  }
  return screen->GetPrimaryDisplay().device_scale_factor();
}

int ConvertBetweenDipAndPx(int value,
                           bool quantize_to_supported_scale_factor,
                           bool invert) {
  float scale = GetPrimaryDisplayScaleFactor();
  if (quantize_to_supported_scale_factor) {
    scale = ui::GetScaleForScaleFactor(ui::GetSupportedScaleFactor(scale));
  }
  DCHECK_NE(0.0f, scale);
  if (invert) {
    scale = 1 / scale;
  }
  return gfx::ScaleToFlooredSize(gfx::Size(value, value), scale).width();
}

}  // namespace

namespace apps_util {

int ConvertDipToPx(int dip, bool quantize_to_supported_scale_factor) {
  return ConvertBetweenDipAndPx(dip, quantize_to_supported_scale_factor, false);
}

int ConvertPxToDip(int px, bool quantize_to_supported_scale_factor) {
  return ConvertBetweenDipAndPx(px, quantize_to_supported_scale_factor, true);
}

ui::ScaleFactor GetPrimaryDisplayUIScaleFactor() {
  return ui::GetSupportedScaleFactor(GetPrimaryDisplayScaleFactor());
}

}  // namespace apps_util
