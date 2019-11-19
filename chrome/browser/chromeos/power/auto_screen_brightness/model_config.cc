// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "chrome/browser/chromeos/power/auto_screen_brightness/model_config.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

ModelConfig::ModelConfig() = default;

ModelConfig::ModelConfig(const ModelConfig& config) = default;

ModelConfig::~ModelConfig() = default;

bool ModelConfig::operator==(const ModelConfig& config) const {
  const double kTol = 1e-10;
  if (std::abs(auto_brightness_als_horizon_seconds -
               config.auto_brightness_als_horizon_seconds) >= kTol)
    return false;

  if (enabled != config.enabled)
    return false;

  if (log_lux.size() != config.log_lux.size())
    return false;

  for (size_t i = 0; i < log_lux.size(); ++i) {
    if (std::abs(log_lux[i] - config.log_lux[i]) >= kTol)
      return false;
  }

  if (brightness.size() != config.brightness.size())
    return false;

  for (size_t i = 0; i < brightness.size(); ++i) {
    if (std::abs(brightness[i] - config.brightness[i]) >= kTol)
      return false;
  }

  if (metrics_key != config.metrics_key)
    return false;

  if (std::abs(model_als_horizon_seconds - config.model_als_horizon_seconds) >=
      kTol)
    return false;

  return true;
}

bool IsValidModelConfig(const ModelConfig& model_config) {
  if (model_config.auto_brightness_als_horizon_seconds <= 0)
    return false;

  if (model_config.log_lux.size() != model_config.brightness.size() ||
      model_config.brightness.size() < 2)
    return false;

  if (model_config.metrics_key.empty())
    return false;

  if (model_config.model_als_horizon_seconds <= 0)
    return false;

  return true;
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
