// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace actor {

void RecordActorTaskStateTransitionActionCount(size_t action_count,
                                               ActorTask::State from_state,
                                               ActorTask::State to_state) {
  base::UmaHistogramCounts1000(
      base::StrCat({"Actor.Task.StateTransition.ActionCount.",
                    ToString(from_state), "_", ToString(to_state)}),
      action_count);
}

void RecordActorTaskStateTransitionDuration(base::TimeDelta duration,
                                            ActorTask::State state) {
  base::UmaHistogramLongTimes100(
      base::StrCat({"Actor.Task.StateTransition.Duration.", ToString(state)}),
      duration);
}

}  // namespace actor
