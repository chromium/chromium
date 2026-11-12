// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/glic/selection/legacy_shake_detector.h"
#include "chrome/browser/glic/selection/wiggle_detector_factory.h"

namespace glic {

std::unique_ptr<WiggleDetector> CreateWiggleDetector(WiggleDetectorType type) {
  switch (type) {
    case WiggleDetectorType::kLegacyShake:
      return std::make_unique<LegacyShakeDetector>();
    case WiggleDetectorType::kLevel0:
    case WiggleDetectorType::kLevel1:
    case WiggleDetectorType::kLevel2:
    case WiggleDetectorType::kLevel3:
      return nullptr;
  }
}

}  // namespace glic
