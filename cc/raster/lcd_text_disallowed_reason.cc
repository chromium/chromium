// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/lcd_text_disallowed_reason.h"

#include <iostream>
#include "base/notreached.h"

namespace cc {

const char* LCDTextDisallowedReasonToString(LCDTextDisallowedReason reason) {
  switch (reason) {
    case LCDTextDisallowedReason::kNone:
      return "none";
    case LCDTextDisallowedReason::kSetting:
      return "setting";
    case LCDTextDisallowedReason::kBackgroundColorNotOpaque:
      return "background-color-not-opaque";
    case LCDTextDisallowedReason::kContentsNotOpaque:
      return "contents-not-opaque";
    case LCDTextDisallowedReason::kNonIntegralTranslation:
      return "non-integral-translation";
    case LCDTextDisallowedReason::kNonIntegralXOffset:
      return "non-integral-x-offset";
    case LCDTextDisallowedReason::kNonIntegralYOffset:
      return "non-integral-y-offset";
    case LCDTextDisallowedReason::kWillChangeTransform:
      return "will-change-transform";
    case LCDTextDisallowedReason::kPixelOrColorEffect:
      return "pixel-or-color-effect";
    case LCDTextDisallowedReason::kTransformAnimation:
      return "transform-animation";
    case LCDTextDisallowedReason::kNoText:
      return "no-text";
  }
  NOTREACHED();
}

std::ostream& operator<<(std::ostream& os, LCDTextDisallowedReason reason) {
  return os << LCDTextDisallowedReasonToString(reason);
}

}  // namespace cc
