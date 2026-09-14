// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_CUSTOMIZATION_UTILS_H_
#define CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_CUSTOMIZATION_UTILS_H_

#include <string>

#include "components/themes/ntp_background_data.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ntp_customization {

// Downsamples the bitmap if width or height exceeds max_dimension by
// halving dimensions to prevent Android Canvas/GPU texture rendering crashes.
SkBitmap DownsampleImageIfNeeded(const SkBitmap& bitmap, int max_dimension);

// Joins non-empty attribution lines with a comma.
std::string GetCustomBackgroundAttribution(const std::string& line_1,
                                           const std::string& line_2);

// Convenience overload to extract the comma-joined attribution string from a
// CustomBackground struct.
std::string GetCustomBackgroundAttribution(const CustomBackground& background);

}  // namespace ntp_customization

#endif  // CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_CUSTOMIZATION_UTILS_H_
