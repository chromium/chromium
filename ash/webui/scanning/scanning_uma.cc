// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/scanning/scanning_uma.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"

namespace ash {
namespace scanning {

ScanJobSettingsResolution GetResolutionEnumValue(const int resolution) {
  switch (resolution) {
    case 75:
      return ScanJobSettingsResolution::k75Dpi;
    case 100:
      return ScanJobSettingsResolution::k100Dpi;
    case 150:
      return ScanJobSettingsResolution::k150Dpi;
    case 200:
      return ScanJobSettingsResolution::k200Dpi;
    case 300:
      return ScanJobSettingsResolution::k300Dpi;
    case 600:
      return ScanJobSettingsResolution::k600Dpi;
    default:
      return ScanJobSettingsResolution::kUnexpectedDpi;
  }
}

}  // namespace scanning
}  // namespace ash
