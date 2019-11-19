// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODEL_CONFIG_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODEL_CONFIG_H_

#include <memory>
#include <string>
#include <vector>

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// Model customization config.
struct ModelConfig {
  double auto_brightness_als_horizon_seconds = -1.0;
  bool enabled = false;
  std::vector<double> log_lux;
  std::vector<double> brightness;
  std::string metrics_key;
  double model_als_horizon_seconds = -1.0;

  ModelConfig();
  ModelConfig(const ModelConfig& config);
  ~ModelConfig();

  bool operator==(const ModelConfig& config) const;
};

bool IsValidModelConfig(const ModelConfig& model_config);

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODEL_CONFIG_H_
