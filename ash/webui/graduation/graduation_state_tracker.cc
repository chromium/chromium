// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/graduation/graduation_state_tracker.h"

#include "base/metrics/histogram_functions.h"

namespace ash::graduation {

GraduationStateTracker::GraduationStateTracker() = default;

GraduationStateTracker::~GraduationStateTracker() {
  base::UmaHistogramEnumeration(kFlowStateHistogramName, flow_state_,
                                FlowState::kNumStates);
}
}  // namespace ash::graduation
