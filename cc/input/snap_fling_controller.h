// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SNAP_FLING_CONTROLLER_H_
#define CC_INPUT_SNAP_FLING_CONTROLLER_H_

#include <memory>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
namespace test {
class SnapFlingControllerTest;
}

class SnapFlingCurve;

// A client that provides information to the controller. It also executes the
// scroll operations and requests animation frames. All the inputs and outputs
// are in the same coordinate space.
class SnapFlingClient {
 public:
  virtual bool GetSnapFlingInfoAndSetSnapTarget(
      const gfx::Vector2dF& natural_displacement,
      gfx::Vector2dF* out_initial_position,
      gfx::Vector2dF* out_target_position) const = 0;
  virtual gfx::Vector2dF ScrollByForSnapFling(const gfx::Vector2dF& delta) = 0;
  virtual void ScrollEndForSnapFling() = 0;
  virtual void RequestAnimationForSnapFling() = 0;
};

// SnapFlingController ensures that an incoming fling event (or inertial-phase
// scroll event) would land on a snap position if there is a valid one nearby.
// It takes an input event, filters it if it conflicts with the current fling,
// or generates a curve if the SnapFlingClient finds a valid snap position.
// It also animates the curve by notifying the client to scroll when clock
// ticks.
class CC_EXPORT SnapFlingController {
 public:
  enum class GestureScrollType { kBegin, kUpdate, kEnd };

  struct GestureScrollUpdateInfo {
    gfx::Vector2dF delta;
    bool is_in_inertial_phase;
    base::TimeTicks event_time;
  };

  explicit SnapFlingController(SnapFlingClient* client);

  static std::unique_ptr<SnapFlingController> CreateForTests(
      SnapFlingClient* client,
      std::unique_ptr<SnapFlingCurve> curve);

  SnapFlingController(const SnapFlingController&) = delete;
  ~SnapFlingController();

  SnapFlingController& operator=(const SnapFlingController&) = delete;

  // Returns true if the event should be consumed for snapping and should not be
  // processed further.
  bool FilterEventForSnap(GestureScrollType gesture_scroll_type);

  // Creates the snap fling curve from the first inertial GSU. Returns true if
  // the event if a snap fling curve has been created and the event should not
  // be processed further.
  bool HandleGestureScrollUpdate(const GestureScrollUpdateInfo& info);

  // Notifies the snap fling controller to update or end the scroll animation.
  void Animate(base::TimeTicks time);

 private:
  friend class test::SnapFlingControllerTest;

  enum class State {
    // We haven't received an inertial GSU in this scroll sequence.
    kIdle,
    // We have received an inertial GSU but decided not to snap for this scroll
    // sequence.
    kIgnored,
    // We have received an inertial GSU and decided to snap and animate it for
    // this scroll sequence. So subsequent GSUs and GSE in the scroll sequence
    // are consumed for snapping.
    kActive,
    // The animation of the snap fling has finished for this scroll sequence.
    // Subsequent GSUs and GSE in the scroll sequence are ignored.
    kFinished,
  };

  SnapFlingController(SnapFlingClient* client,
                      std::unique_ptr<SnapFlingCurve> curve);
  void ClearSnapFling();

  // Sets the |curve_| to |curve| and the |state| to |kActive|.
  void SetCurveForTest(std::unique_ptr<SnapFlingCurve> curve);

  void SetActiveStateForTest() { state_ = State::kActive; }

  SnapFlingClient* client_;
  State state_ = State::kIdle;
  std::unique_ptr<SnapFlingCurve> curve_;
};

}  // namespace cc

#endif  // CC_INPUT_SNAP_FLING_CONTROLLER_H_
