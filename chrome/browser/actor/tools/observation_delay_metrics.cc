// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/observation_delay_metrics.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"

namespace actor {

const char kActorObservationDelayStateDurationWaitForPageStabilityMetricName[] =
    "Actor.ObservationDelay.StateDuration.WaitForPageStability";

const char
    kActorObservationDelayStateDurationWaitForLoadCompletionMetricName[] =
        "Actor.ObservationDelay.StateDuration.WaitForLoadCompletion";

const char
    kActorObservationDelayStateDurationWaitForVisualStateUpdateMetricName[] =
        "Actor.ObservationDelay.StateDuration.WaitForVisualStateUpdate";

const char kActorObservationDelayTotalWaitDurationMetricName[] =
    "Actor.ObservationDelay.TotalWaitDuration";

const char kActorObservationDelayDidTimeoutMetricName[] =
    "Actor.ObservationDelay.DidTimeout";

const char kActorObservationDelayLcpDelayNeededMetricName[] =
    "Actor.ObservationDelay.LcpDelayNeeded";

ObservationDelayMetrics::ObservationDelayMetrics() = default;

ObservationDelayMetrics::~ObservationDelayMetrics() = default;

void ObservationDelayMetrics::Start() {
  wait_start_time_ = base::TimeTicks::Now();
}

void ObservationDelayMetrics::WillMoveToState(
    ObservationDelayController::State state) {
  base::TimeTicks now = base::TimeTicks::Now();

  switch (state) {
    case ObservationDelayController::State::kInitial:
      NOTREACHED();
    case ObservationDelayController::State::kWaitForPageStability:
      wait_for_page_stability_.start_time = now;
      break;
    case ObservationDelayController::State::kPageStabilityMonitorDisconnected:
      break;
    case ObservationDelayController::State::kWaitForLoadCompletion:
      wait_for_load_completion_.start_time = now;
      break;
    case ObservationDelayController::State::kWaitForVisualStateUpdate:
      wait_for_visual_state_update_.start_time = now;
      break;
    case ObservationDelayController::State::kMaybeDelayForLcp:
      delay_for_lcp_ = false;
      break;
    case ObservationDelayController::State::kDelayForLcp:
      delay_for_lcp_ = true;
      break;
    case ObservationDelayController::State::kDidTimeout:
      did_timeout = true;
      break;
    case ObservationDelayController::State::kDone:
      base::UmaHistogramBoolean(kActorObservationDelayDidTimeoutMetricName,
                                did_timeout);

      if (!did_timeout) {
        CHECK(!wait_start_time_.is_null());
        base::UmaHistogramTimes(
            kActorObservationDelayTotalWaitDurationMetricName,
            base::TimeTicks::Now() - wait_start_time_);

        if (wait_for_page_stability_.IsValid()) {
          base::UmaHistogramTimes(
              kActorObservationDelayStateDurationWaitForPageStabilityMetricName,
              wait_for_page_stability_.end_time -
                  wait_for_page_stability_.start_time);
        }
        if (wait_for_load_completion_.IsValid()) {
          base::UmaHistogramTimes(
              kActorObservationDelayStateDurationWaitForLoadCompletionMetricName,
              wait_for_load_completion_.end_time -
                  wait_for_load_completion_.start_time);
        }
        if (wait_for_visual_state_update_.IsValid()) {
          base::UmaHistogramTimes(
              kActorObservationDelayStateDurationWaitForVisualStateUpdateMetricName,
              wait_for_visual_state_update_.end_time -
                  wait_for_visual_state_update_.start_time);
        }

        if (delay_for_lcp_.has_value()) {
          base::UmaHistogramBoolean(
              kActorObservationDelayLcpDelayNeededMetricName, *delay_for_lcp_);
        }
      }
      break;
  }
}

void ObservationDelayMetrics::OnPageStable() {
  wait_for_page_stability_.end_time = base::TimeTicks::Now();
  CHECK(wait_for_page_stability_.IsValid());
}

void ObservationDelayMetrics::OnLoadCompleted() {
  wait_for_load_completion_.end_time = base::TimeTicks::Now();
  CHECK(wait_for_load_completion_.IsValid());
}

void ObservationDelayMetrics::OnVisualStateUpdated() {
  wait_for_visual_state_update_.end_time = base::TimeTicks::Now();
  CHECK(wait_for_visual_state_update_.IsValid());
}

}  // namespace actor
