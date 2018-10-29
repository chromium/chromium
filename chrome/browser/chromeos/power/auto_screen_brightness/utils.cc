// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

double ConvertToLog(double value) {
  return std::log(1 + value);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
