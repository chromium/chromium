// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_DIP_PX_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_DIP_PX_UTIL_H_

// Utility functions for converting between DIP (device independent pixels) and
// PX (physical pixels).
//
// "Supported scale factor" means a ui::ScaleFactor enum value (representing
// one of a finite number of floating point values) returned by
// ui::GetSupportedScaleFactor, defined in ui/base/layout.h.

#include "ui/base/resource/scale_factor.h"

namespace apps_util {

int ConvertDipToPx(int dip, bool quantize_to_supported_scale_factor);
int ConvertPxToDip(int px, bool quantize_to_supported_scale_factor);
ui::ScaleFactor GetPrimaryDisplayUIScaleFactor();

}  // namespace apps_util

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_DIP_PX_UTIL_H_
