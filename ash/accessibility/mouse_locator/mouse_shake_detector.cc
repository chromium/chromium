// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/mouse_locator/mouse_shake_detector.h"

namespace ash {

MouseShakeDetector::MouseShakeDetector() = default;

MouseShakeDetector::~MouseShakeDetector() = default;

void MouseShakeDetector::ProcessPosition(const gfx::PointF& location_in_screen,
                                         base::TimeTicks event_time) {
  // TODO(b/414450865): Track stroke direction and record reversals. Notify
  // observers when a shake starts, and restart `shake_end_timer_` on each
  // reversal while shaking.
}

void MouseShakeDetector::Reset() {
  // TODO(b/414450865): Clear the stroke and reversal state, and end the shake
  // if one is in progress.
}

void MouseShakeDetector::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MouseShakeDetector::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MouseShakeDetector::InitializeStroke(const gfx::PointF& location_in_screen,
                                          base::TimeTicks event_time) {
  // TODO(b/414450865): Anchor a new stroke at `location_in_screen`.
}

void MouseShakeDetector::PruneOldReversals(base::TimeTicks event_time) {
  // TODO(b/414450865): Drop reversals older than the detection window.
}

void MouseShakeDetector::OnShakeEndTimerFired() {
  // TODO(b/414450865): Notify observers that the shake has ended.
}

}  // namespace ash
