// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_LCD_TEXT_DISALLOWED_REASON_H_
#define CC_RASTER_LCD_TEXT_DISALLOWED_REASON_H_

#include <cstddef>
#include <cstdint>
#include <iosfwd>

#include "cc/cc_export.h"

namespace cc {

// These values are used in UMA and benchmarks. Entries should not be renumbered
// and numeric values should never be reused.
enum class LCDTextDisallowedReason : uint8_t {
  kNone = 0,
  kSetting = 1,
  kBackgroundColorNotOpaque = 2,
  kContentsNotOpaque = 3,
  kNonIntegralTranslation = 4,
  kNonIntegralXOffset = 5,
  kNonIntegralYOffset = 6,
  kWillChangeTransform = 7,
  kPixelOrColorEffect = 8,
  kTransformAnimation = 9,
  kNoText = 10,
  kMaxValue = kNoText,
};
constexpr size_t kLCDTextDisallowedReasonCount =
    static_cast<size_t>(LCDTextDisallowedReason::kMaxValue) + 1;
CC_EXPORT const char* LCDTextDisallowedReasonToString(LCDTextDisallowedReason);

CC_EXPORT std::ostream& operator<<(std::ostream&, LCDTextDisallowedReason);

}  // namespace cc

#endif  // CC_RASTER_LCD_TEXT_DISALLOWED_REASON_H_
