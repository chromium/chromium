// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_WATERMARK_SETTINGS_H_
#define CHROME_BROWSER_ENTERPRISE_WATERMARK_SETTINGS_H_

#include "third_party/skia/include/core/SkColor.h"

class PrefService;

namespace enterprise_watermark {

// Base RGB colors for the watermark text.
inline constexpr SkColor kBaseFillRGB =
    SkColorSetRGB(0x00, 0x00, 0x00);  // Black
inline constexpr SkColor kBaseOutlineRGB =
    SkColorSetRGB(0xff, 0xff, 0xff);  // White

// Helper function to convert a percentage (0-100) to SkAlpha (0-255).
SkAlpha PercentageToSkAlpha(int percent_value);

SkColor GetDefaultFillColor();
SkColor GetDefaultOutlineColor();
int GetDefaultFontSize();
std::string GetDefaultTimestampTimezone();
SkColor GetFillColor(const PrefService* prefs);
SkColor GetOutlineColor(const PrefService* prefs);

// Log values for type of timezone in watermark timestamps. This is used to log
// metrics as WatermarkTimestampTimezoneType, so numeric values must not be
// changed.
enum class TimestampTimezoneType {
  // Default user's device wall-clock time.
  kDefault = 0,

  // Admin-configured user's device wall-clock time.
  kUserDevice = 1,

  // Admin-configured valid IANA time zone.
  kValidIANATimeZone = 2,

  // Invalid string received, fell back to default (user's device time).
  kInvalidFallback = 3,

  kMaxValue = kInvalidFallback,
};

// Returns the font size for the watermark.
// This function always returns a positive integer (>= 1).
int GetFontSize(const PrefService* prefs);
std::string GetTimestampTimezone(const PrefService* prefs);

}  // namespace enterprise_watermark

#endif  // CHROME_BROWSER_ENTERPRISE_WATERMARK_SETTINGS_H_
