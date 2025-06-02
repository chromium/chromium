// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_TYPE_H_
#define CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_TYPE_H_

namespace actor {

enum class ObservationDelayType {
  // Observation is scheduled immediately after tool use completes.
  kNone,

  // Observation is scheduled a fixed delay after tool use completes.
  // TODO(crbug.com/414662842): This can be removed once renderer-side tools are
  // smart enough to observe page readiness signals.
  kUseCompletionDelay,

  // The page is observed for loading (cross-document) navigations. If one is
  // started, observation is delayed until the page completes loading. Whether
  // or not a navigation is started, the observation is taken only after a new
  // frame for the page is presented.
  kWatchForLoad,
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_TYPE_H_
