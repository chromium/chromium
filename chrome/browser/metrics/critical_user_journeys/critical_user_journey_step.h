// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_STEP_H_
#define CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_STEP_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace metrics {

class CriticalUserJourney;

// Represents a single stage in a Critical User Journey (CUJ).
// Maps a UI interaction to a metric ID to track user progress. Supports
// branching into sub-journeys and optional timeouts for the step.
class CriticalUserJourneyStep : public ui::InteractionSequence::Step {
 public:
  CriticalUserJourneyStep();
  ~CriticalUserJourneyStep();

  int metric_id = 0;
  std::vector<std::unique_ptr<CriticalUserJourney>> branches;
  base::TimeDelta time_out_duration = base::TimeDelta();
  ui::InteractionSequence::SubsequenceMode mode =
      ui::InteractionSequence::SubsequenceMode::kExactlyOne;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_STEP_H_
