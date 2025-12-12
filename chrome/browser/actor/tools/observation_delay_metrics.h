// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_METRICS_H_
#define CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_METRICS_H_

#include <optional>

#include "base/time/time.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"

namespace actor {

extern const char
    kActorObservationDelayStateDurationWaitForPageStabilityMetricName[];
extern const char
    kActorObservationDelayStateDurationWaitForLoadCompletionMetricName[];
extern const char
    kActorObservationDelayStateDurationWaitForVisualStateUpdateMetricName[];
extern const char kActorObservationDelayTotalWaitDurationMetricName[];
extern const char kActorObservationDelayDidTimeoutMetricName[];
extern const char kActorObservationDelayLcpDelayNeededMetricName[];
extern const char kActorObservationDelayDidNavigateMetricName[];

class ObservationDelayMetrics {
 public:
  ObservationDelayMetrics();
  ~ObservationDelayMetrics();

  void Start();

  void WillMoveToState(ObservationDelayController::State state);

  void OnPageStable();

  void OnLoadCompleted();

  void OnVisualStateUpdated();

 private:
  struct StateDuration {
    bool IsValid() const {
      return !start_time.is_null() && !end_time.is_null();
    }
    base::TimeTicks start_time;
    base::TimeTicks end_time;
  };

  void RecordStateDuration(ObservationDelayController::State state);

  // The time at which it starts to wait for page stability/page
  // loading.
  base::TimeTicks wait_start_time_;

  // The duration waiting for page stability.
  StateDuration wait_for_page_stability_;

  // The duration waiting for page loading.
  StateDuration wait_for_load_completion_;

  // The duration waiting for visual state update.
  StateDuration wait_for_visual_state_update_;

  // Whether the observation delay completed due to timeout.
  bool did_timeout_ = false;

  // Whether the observation delay completed due to navigation.
  bool did_navigate_ = false;

  // Whether additional delay is applied to wait for LCP. Will be `std::nullopt`
  // until `kMaybeDelayForLCP` state is entered.
  std::optional<bool> delay_for_lcp_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_METRICS_H_
