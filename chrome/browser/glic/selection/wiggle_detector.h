// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_WIGGLE_DETECTOR_H_
#define CHROME_BROWSER_GLIC_SELECTION_WIGGLE_DETECTOR_H_

#include "base/time/time.h"
#include "ui/gfx/geometry/point_f.h"

namespace glic {

// Detects a cursor wiggle from a stream of pointer positions.
class WiggleDetector {
 public:
  virtual ~WiggleDetector() = default;

  // Adds a pointer position in DIPs, seen at `timestamp`. Returns true when a
  // wiggle is detected; the detector then resets itself.
  virtual bool AddSample(const gfx::PointF& position_in_dips,
                         base::TimeTicks timestamp) = 0;

  // Clears all state (called on clicks, key presses, scrolls, etc.).
  virtual void Reset() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_WIGGLE_DETECTOR_H_
