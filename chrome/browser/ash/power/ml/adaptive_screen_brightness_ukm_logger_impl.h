// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_UKM_LOGGER_IMPL_H_
#define CHROME_BROWSER_ASH_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_UKM_LOGGER_IMPL_H_

#include "chrome/browser/ash/power/ml/adaptive_screen_brightness_ukm_logger.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ash {
namespace power {
namespace ml {

class AdaptiveScreenBrightnessUkmLoggerImpl
    : public AdaptiveScreenBrightnessUkmLogger {
 public:
  AdaptiveScreenBrightnessUkmLoggerImpl() = default;

  AdaptiveScreenBrightnessUkmLoggerImpl(
      const AdaptiveScreenBrightnessUkmLoggerImpl&) = delete;
  AdaptiveScreenBrightnessUkmLoggerImpl& operator=(
      const AdaptiveScreenBrightnessUkmLoggerImpl&) = delete;

  ~AdaptiveScreenBrightnessUkmLoggerImpl() override;

  // ash::power::ml::AdaptiveScreenBrightnessUkmLogger overrides:
  void LogActivity(const ScreenBrightnessEvent& screen_brightness_event,
                   ukm::SourceId tab_id,
                   bool has_form_entry) override;

 private:
  // This ID is incremented each time a ScreenBrightessEvent is logged to UKM.
  // Event index resets when a new user session starts.
  int next_sequence_id_ = 1;
};

}  // namespace ml
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_UKM_LOGGER_IMPL_H_
