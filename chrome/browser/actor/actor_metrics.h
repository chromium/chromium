// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_METRICS_H_
#define CHROME_BROWSER_ACTOR_ACTOR_METRICS_H_

#include <cstddef>

#include "chrome/browser/actor/actor_task.h"

namespace actor {

// Records the number of actions taken in `from_state` before transitioning to
// `to_state`.
void RecordActorTaskStateTransitionActionCount(size_t action_count,
                                               ActorTask::State from_state,
                                               ActorTask::State to_state);

// Records the duration spent in `state`.
void RecordActorTaskStateTransitionDuration(base::TimeDelta duration,
                                            ActorTask::State state);

}  // namespace actor
#endif  // CHROME_BROWSER_ACTOR_ACTOR_METRICS_H_
