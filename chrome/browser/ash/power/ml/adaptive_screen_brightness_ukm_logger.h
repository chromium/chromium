// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_UKM_LOGGER_H_
#define CHROME_BROWSER_ASH_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_UKM_LOGGER_H_

#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ash {
namespace power {
namespace ml {

class ScreenBrightnessEvent;

// Interface to log a ScreenBrightnessEvent to UKM.
class AdaptiveScreenBrightnessUkmLogger {
 public:
  virtual ~AdaptiveScreenBrightnessUkmLogger() = default;

  // Log screen brightness proto.
  // |tab_id| is the UKM SourceId associated with the active tab in the focused
  // visible browser (or topmost visible browser if none are focused).
  virtual void LogActivity(const ScreenBrightnessEvent& screen_brightness,
                           ukm::SourceId tab_id,
                           bool has_form_entry) = 0;
};

}  // namespace ml
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_UKM_LOGGER_H_
