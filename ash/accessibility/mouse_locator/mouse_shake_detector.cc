// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/mouse_locator/mouse_shake_detector.h"

#include <cstddef>

#include "base/check.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ash {

namespace {

// A shake starts once `kRequiredReversalCount` reversals are confirmed
// within a sliding window of this length. Older reversals no longer count.
constexpr base::TimeDelta kActivationWindow = base::Milliseconds(1000);
constexpr size_t kRequiredReversalCount = 10;

// A shake ends when no reversal is confirmed for this long.
constexpr base::TimeDelta kShakeEndTimeout = base::Milliseconds(250);

// Distance thresholds. Evaluating physical displacement rather than
// per-event velocity ensures behavior is invariant to input device polling
// rate. They are stored squared because they are only compared against
// squared vector lengths.

// A turn counts as a reversal only if the stroke before it covered at least
// 24 DIP. Shorter strokes are treated as twitches.
constexpr float kMinimumStrokeDistanceSquared = 24 * 24;

// Once the pointer stops advancing, it must move at least 8 DIP from the
// stroke extreme before the turn is evaluated, so that jitter at the extreme
// is ignored.
constexpr float kReversalConfirmationDistanceSquared = 8 * 8;

// A newly anchored stroke has no direction until the pointer moves at least
// 6 DIP from the anchor.
constexpr float kInitialDirectionDistanceSquared = 6 * 6;

// The return direction must be at least 120 degrees away from the completed
// stroke direction: cos(120 degrees) = -0.5.
constexpr float kMaximumReversalCosine = -0.5f;

// Returns cos(theta), where theta is the angle between the vectors a and b:
//
//                  a . b
//   cos(theta) = ---------
//                 |a| |b|
double CosineBetween(const gfx::Vector2dF& a, const gfx::Vector2dF& b) {
  const double magnitude = a.Length() * b.Length();
  CHECK_GT(magnitude, 0.0);
  return gfx::DotProduct(a, b) / magnitude;
}

}  // namespace

MouseShakeDetector::MouseShakeDetector() = default;

MouseShakeDetector::~MouseShakeDetector() = default;

void MouseShakeDetector::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MouseShakeDetector::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MouseShakeDetector::ProcessPosition(const gfx::PointF& location_in_screen,
                                         base::TimeTicks event_time) {
  if (!stroke_initialized_) {
    InitializeStroke(location_in_screen, event_time);
    return;
  }

  // Never connect geometry across a long gap between events; by then every
  // recorded reversal has also aged out. The next event establishes a fresh
  // stroke anchor.
  if (event_time - last_event_time_ > kActivationWindow) {
    Reset();
    InitializeStroke(location_in_screen, event_time);
    return;
  }
  last_event_time_ = event_time;
  PruneOldReversals(event_time);

  gfx::Vector2dF stroke_direction = stroke_extreme_ - stroke_start_;
  if (stroke_direction.LengthSquared() < kInitialDirectionDistanceSquared) {
    const gfx::Vector2dF candidate_direction =
        location_in_screen - stroke_start_;
    if (candidate_direction.LengthSquared() >=
        kInitialDirectionDistanceSquared) {
      stroke_extreme_ = location_in_screen;
    }
    return;
  }

  const gfx::Vector2dF movement_from_extreme =
      location_in_screen - stroke_extreme_;

  // Positive projection means the pointer is still progressing along the same
  // stroke. Use the whole start-to-extreme chord as the stable direction, not
  // the most recent device-dependent event delta.
  const double forward_projection =
      gfx::DotProduct(stroke_direction, movement_from_extreme);
  if (forward_projection > 0.0) {
    stroke_extreme_ = location_in_screen;
    return;
  }

  if (movement_from_extreme.LengthSquared() <
      kReversalConfirmationDistanceSquared) {
    return;
  }

  // Any significant non-forward turn begins new geometry at the old extreme.
  // This includes a sideways turn and a rejected short twitch. Keeping the old
  // direction in either case can make a later movement fabricate or miss a
  // reversal against stale geometry.
  const gfx::PointF previous_extreme = stroke_extreme_;
  stroke_start_ = previous_extreme;
  stroke_extreme_ = location_in_screen;

  const bool direction_reversed =
      CosineBetween(stroke_direction, movement_from_extreme) <=
      kMaximumReversalCosine;
  const bool completed_stroke =
      stroke_direction.LengthSquared() >= kMinimumStrokeDistanceSquared;
  if (!direction_reversed || !completed_stroke) {
    return;
  }

  OnReversal(location_in_screen, event_time);
}

void MouseShakeDetector::OnReversal(const gfx::PointF& location_in_screen,
                                    base::TimeTicks event_time) {
  // While shaking, every reversal postpones the end of the shake.
  if (is_shaking_) {
    shake_end_timer_.Reset();
    return;
  }

  recent_reversal_times_.push_back(event_time);
  if (recent_reversal_times_.size() < kRequiredReversalCount) {
    return;
  }

  recent_reversal_times_.clear();
  is_shaking_ = true;
  shake_end_timer_.Start(FROM_HERE, kShakeEndTimeout, this,
                         &MouseShakeDetector::OnShakeEndTimerFired);
  NotifyMouseShakeStarted(location_in_screen);
}

void MouseShakeDetector::OnShakeEndTimerFired() {
  CHECK(is_shaking_);
  // Rearm from scratch: a new shake needs a full set of fresh reversals.
  Reset();
}

void MouseShakeDetector::InitializeStroke(const gfx::PointF& location_in_screen,
                                          base::TimeTicks event_time) {
  stroke_initialized_ = true;
  stroke_start_ = location_in_screen;
  stroke_extreme_ = location_in_screen;
  last_event_time_ = event_time;
}

void MouseShakeDetector::PruneOldReversals(base::TimeTicks event_time) {
  while (!recent_reversal_times_.empty() &&
         event_time - recent_reversal_times_.front() > kActivationWindow) {
    recent_reversal_times_.pop_front();
  }
}

void MouseShakeDetector::Reset() {
  recent_reversal_times_.clear();
  stroke_start_ = gfx::PointF();
  stroke_extreme_ = gfx::PointF();
  last_event_time_ = base::TimeTicks();
  stroke_initialized_ = false;

  shake_end_timer_.Stop();
  if (!is_shaking_) {
    return;
  }
  is_shaking_ = false;
  NotifyMouseShakeEnded();
}

void MouseShakeDetector::NotifyMouseShakeStarted(
    const gfx::PointF& location_in_screen) {
  for (Observer& observer : observers_) {
    observer.OnMouseShakeStarted(location_in_screen);
  }
}

void MouseShakeDetector::NotifyMouseShakeEnded() {
  for (Observer& observer : observers_) {
    observer.OnMouseShakeEnded();
  }
}

}  // namespace ash
