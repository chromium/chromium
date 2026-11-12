// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_LEGACY_SHAKE_DETECTOR_H_
#define CHROME_BROWSER_GLIC_SELECTION_LEGACY_SHAKE_DETECTOR_H_

#include <optional>

#include "base/time/time.h"
#include "chrome/browser/glic/selection/wiggle_detector.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace glic {

// Detects a shake as a few quick direction reversals of the pointer.
class LegacyShakeDetector : public WiggleDetector {
 public:
  LegacyShakeDetector();
  LegacyShakeDetector(const LegacyShakeDetector&) = delete;
  LegacyShakeDetector& operator=(const LegacyShakeDetector&) = delete;
  ~LegacyShakeDetector() override;

  // WiggleDetector:
  bool AddSample(const gfx::PointF& position_in_dips,
                 base::TimeTicks timestamp) override;
  void Reset() override;

 private:
  std::optional<gfx::PointF> last_point_;
  std::optional<gfx::Vector2dF> last_direction_;
  int direction_change_count_ = 0;
  base::TimeTicks last_direction_change_time_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_LEGACY_SHAKE_DETECTOR_H_
