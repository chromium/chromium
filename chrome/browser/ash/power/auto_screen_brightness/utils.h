// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_UTILS_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_UTILS_H_

#include <string>
#include <vector>

#include "base/containers/ring_buffer.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DataError {
  kAlsValue = 0,
  kBrightnessPercent = 1,
  kMojoSamplesObserver = 2,
  kMaxValue = kMojoSamplesObserver
};

// Logs data errors to UMA.
void LogDataError(DataError error);

// Returns natural log of 1+|value|.
double ConvertToLog(double value);

// Represents whether any trainer or adapter parameter has been set incorrectly.
// This does *not* include the status of the user's personal curve.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ParameterError {
  kModelError = 0,
  kAdapterError = 1,
  kMaxValue = kAdapterError
};

// Logs to UMA that a parameter is invalid.
void LogParameterError(ParameterError error);

// Formats a double value that represents percentages.
std::string FormatToPrint(double value);

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_UTILS_H_
