// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_MOUSE_LOCATOR_MOUSE_SHAKE_DETECTOR_H_
#define ASH_ACCESSIBILITY_MOUSE_LOCATOR_MOUSE_SHAKE_DETECTOR_H_

#include "ash/ash_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

// Detects a rapid back-and-forth "shake" of the mouse cursor and notifies
// observers when a shake starts and ends. No event marks the end of a shake, so
// it ends when no new reversal arrives within a short timeout.
// Observers should manage their lifetime using base::ScopedObservation.
class ASH_EXPORT MouseShakeDetector {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a shake is detected.
    virtual void OnMouseShakeStarted(const gfx::PointF& location_in_screen) {}

    // Called when the shake stops, or when Reset() is called during a shake.
    virtual void OnMouseShakeEnded() {}

   protected:
    ~Observer() override = default;
  };

  MouseShakeDetector();
  MouseShakeDetector(const MouseShakeDetector&) = delete;
  MouseShakeDetector& operator=(const MouseShakeDetector&) = delete;
  ~MouseShakeDetector();

  // Evaluates a cursor position in screen coordinates.
  void ProcessPosition(const gfx::PointF& location_in_screen,
                       base::TimeTicks event_time);

  // Clears all tracking state, ending the shake if one is in progress.
  void Reset();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Begins a new stroke anchored at `location_in_screen`.
  void InitializeStroke(const gfx::PointF& location_in_screen,
                        base::TimeTicks event_time);

  // Drops reversals that have aged out of the detection window.
  void PruneOldReversals(base::TimeTicks event_time);

  // Ends the shake when no new reversal arrived within the timeout.
  void OnShakeEndTimerFired();

  // TODO(b/414450865): Add the stroke and reversal tracking state.

  base::OneShotTimer shake_end_timer_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_MOUSE_LOCATOR_MOUSE_SHAKE_DETECTOR_H_
