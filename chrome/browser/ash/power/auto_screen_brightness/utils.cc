// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/utils.h"
#include "base/strings/stringprintf.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

double ConvertToLog(double value) {
  return std::log(1 + value);
}

std::string FormatToPrint(double value) {
  return base::StringPrintf("%.4f", value) + "%";
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
