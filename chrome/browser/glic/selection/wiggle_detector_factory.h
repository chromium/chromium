// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_WIGGLE_DETECTOR_FACTORY_H_
#define CHROME_BROWSER_GLIC_SELECTION_WIGGLE_DETECTOR_FACTORY_H_

#include <memory>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "chrome/browser/glic/selection/wiggle_detector.h"

namespace glic {

// Recorded in UMA (Glic.Wiggle.Detected). Don't renumber.
// Lower levels are less sensitive.
// LINT.IfChange(GlicWiggleDetectorType)
enum class WiggleDetectorType {
  kLevel0 = 0,
  kLevel1 = 1,
  kLevel2 = 2,
  kLevel3 = 3,
  kLegacyShake = 4,
  kMaxValue = kLegacyShake,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicWiggleDetectorType)

// kLegacyShake, plus the levels in branded builds.
base::span<const WiggleDetectorType> GetAvailableWiggleDetectorTypes();

// How long to ignore the pointer after a detection, after a click, and after
// it rests for `pause_threshold`.
struct WiggleTiming {
  base::TimeDelta cooldown;
  base::TimeDelta ignore_after_click;
  base::TimeDelta pause_threshold;
  base::TimeDelta ignore_after_pause;
};
WiggleTiming GetDefaultWiggleTiming();

// `type` must be in GetAvailableWiggleDetectorTypes().
std::unique_ptr<WiggleDetector> CreateWiggleDetector(WiggleDetectorType type);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_WIGGLE_DETECTOR_FACTORY_H_
