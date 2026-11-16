// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/glic/selection/legacy_shake_detector.h"
#include "chrome/browser/glic/selection/wiggle_detector_factory.h"

namespace glic {

base::span<const WiggleDetectorType> GetAvailableWiggleDetectorTypes() {
  static constexpr WiggleDetectorType kTypes[] = {
      WiggleDetectorType::kLegacyShake};
  return kTypes;
}

WiggleTiming GetDefaultWiggleTiming() {
  return {.cooldown = base::TimeDelta(),
          .ignore_after_click = base::TimeDelta(),
          .pause_threshold = base::TimeDelta::Max(),
          .ignore_after_pause = base::TimeDelta()};
}

std::unique_ptr<WiggleDetector> CreateWiggleDetector(WiggleDetectorType type) {
  switch (type) {
    case WiggleDetectorType::kLegacyShake:
      return std::make_unique<LegacyShakeDetector>();
    case WiggleDetectorType::kLevel0:
    case WiggleDetectorType::kLevel1:
    case WiggleDetectorType::kLevel2:
    case WiggleDetectorType::kLevel3:
      NOTREACHED() << "Not available in this build";
  }
}

}  // namespace glic
