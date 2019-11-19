// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/snap_fling_controller.h"

#include "cc/input/snap_fling_curve.h"

namespace cc {

SnapFlingController::SnapFlingController(SnapFlingClient* client)
    : client_(client), state_(State::kIdle) {}

SnapFlingController::~SnapFlingController() = default;

bool SnapFlingController::FilterEventForSnap(
    SnapFlingController::GestureScrollType gesture_scroll_type) {
  switch (gesture_scroll_type) {
    case GestureScrollType::kBegin: {
      ClearSnapFling();
      return false;
    }
    // TODO(sunyunjia): Need to update the existing snap curve if the GSU is
    // from a fling boosting event.
    case GestureScrollType::kUpdate:
    case GestureScrollType::kEnd: {
      return state_ == State::kActive || state_ == State::kFinished;
    }
  }
}

void SnapFlingController::ClearSnapFling() {
  if (state_ == State::kActive)
    client_->ScrollEndForSnapFling();

  curve_.reset();
  state_ = State::kIdle;
}

bool SnapFlingController::HandleGestureScrollUpdate(
    const SnapFlingController::GestureScrollUpdateInfo& info) {
  DCHECK(state_ != State::kActive && state_ != State::kFinished);
  if (state_ != State::kIdle)
    return false;

  if (!info.is_in_inertial_phase)
    return false;

  gfx::Vector2dF ending_displacement =
      SnapFlingCurve::EstimateDisplacement(info.delta);

  gfx::Vector2dF target_offset, start_offset;
  if (!client_->GetSnapFlingInfoAndSetSnapTarget(
          ending_displacement, &start_offset, &target_offset)) {
    state_ = State::kIgnored;
    return false;
  }

  if (start_offset == target_offset) {
    state_ = State::kFinished;
    return true;
  }

  curve_ = std::make_unique<SnapFlingCurve>(start_offset, target_offset,
                                            info.event_time);
  state_ = State::kActive;
  Animate(info.event_time);
  return true;
}

void SnapFlingController::Animate(base::TimeTicks time) {
  if (state_ != State::kActive)
    return;

  if (curve_->IsFinished()) {
    client_->ScrollEndForSnapFling();
    state_ = State::kFinished;
    return;
  }
  gfx::Vector2dF snapped_delta = curve_->GetScrollDelta(time);
  gfx::Vector2dF current_offset = client_->ScrollByForSnapFling(snapped_delta);
  curve_->UpdateCurrentOffset(current_offset);
  client_->RequestAnimationForSnapFling();
}

void SnapFlingController::SetCurveForTest(
    std::unique_ptr<SnapFlingCurve> curve) {
  curve_ = std::move(curve);
  state_ = State::kActive;
}

}  // namespace cc
