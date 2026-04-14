// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_SESSION_H_
#define CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_SESSION_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace ui {
class TrackedElement;
}

namespace metrics {

class CriticalUserJourney;

// Manages a single active instance of a Critical User Journey.
// Uses ui::InteractionSequence to track progress through steps.
class CriticalUserJourneySession {
 public:
  // LINT.IfChange(JourneyResult)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class JourneyResult {
    kCompleted = 0,
    kAborted = 1,
    kTimeout = 2,
    kMaxValue = kTimeout,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/critical_user_journeys/enums.xml:CriticalUserJourneyResult)

  explicit CriticalUserJourneySession(const CriticalUserJourney* journey);
  ~CriticalUserJourneySession();

  CriticalUserJourneySession(const CriticalUserJourneySession&) = delete;
  CriticalUserJourneySession& operator=(const CriticalUserJourneySession&) =
      delete;

  // Starts the journey. If |element| is provided, it is treated as the trigger
  // that already matched the first step of the journey, with the specified
  // metric ID.
  void Start(std::optional<int> first_step_metric_id,
             ui::TrackedElement* element);

  void set_on_done_callback(
      base::OnceCallback<void(JourneyResult)> on_done_callback) {
    on_done_callback_ = std::move(on_done_callback);
  }

  const CriticalUserJourney* journey() { return journey_; }

 private:
  // Build the underlying InteractionSequence::Builder from the journey
  // definition.
  ui::InteractionSequence::Builder BuildSequence(
      ui::ElementContext context,
      const CriticalUserJourney* journey,
      bool is_root,
      std::optional<int> first_step_metric_id,
      ui::TrackedElement* initial_element);

  // Callbacks for InteractionSequence events.
  void OnStepStarted(int metric_id, base::TimeDelta timeout);
  void OnTimeout();
  void OnAborted(const ui::InteractionSequence::AbortedData& data);
  void OnCompleted();

  const raw_ptr<const CriticalUserJourney> journey_;
  base::OnceCallback<void(JourneyResult)> on_done_callback_;
  std::unique_ptr<ui::InteractionSequence> sequence_;

  static constexpr int kNoMetricId = -1;

  int last_reached_metric_id_ = kNoMetricId;
  base::TimeTicks journey_start_time_;
  base::TimeTicks last_step_time_;
  base::OneShotTimer timeout_timer_;
  bool was_timeout_ = false;

  base::WeakPtrFactory<CriticalUserJourneySession> weak_factory_{this};
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_SESSION_H_
