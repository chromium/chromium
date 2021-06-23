// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_POWER_DETAILS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_POWER_POWER_DETAILS_PROVIDER_H_

#include <memory>

// Used to retrieve some power related metrics.
class PowerDetailsProvider {
 public:
  static constexpr double kInvalidScreenBrightness = -1.0;

  // Creates a platform specific PowerDetailsProvider.
  static std::unique_ptr<PowerDetailsProvider> Create();

  virtual ~PowerDetailsProvider() = default;

  PowerDetailsProvider(const PowerDetailsProvider& other) = delete;
  PowerDetailsProvider& operator=(const PowerDetailsProvider& other) = delete;

  // Returns the brightness of the main screen when available,
  // |kInvalidScreenBrightness| otherwise.
  virtual double GetMainScreenBrightnessLevel() = 0;

 protected:
  PowerDetailsProvider() = default;
};

#endif  // CHROME_BROWSER_METRICS_POWER_POWER_DETAILS_PROVIDER_H_
