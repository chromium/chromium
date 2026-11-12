// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_WIGGLE_DETECTOR_FACTORY_H_
#define CHROME_BROWSER_GLIC_SELECTION_WIGGLE_DETECTOR_FACTORY_H_

#include <memory>

#include "chrome/browser/glic/selection/wiggle_detector.h"

namespace glic {

// Recorded in UMA (Glic.Wiggle.Detected). Don't renumber.
enum class WiggleDetectorType {
  kLevel0 = 0,
  kLevel1 = 1,
  kLevel2 = 2,
  kLevel3 = 3,
  kLegacyShake = 4,
  kMaxValue = kLegacyShake,
};

// Returns a new detector of `type`, or nullptr if this build doesn't have it.
std::unique_ptr<WiggleDetector> CreateWiggleDetector(WiggleDetectorType type);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_WIGGLE_DETECTOR_FACTORY_H_
