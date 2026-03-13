// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_SESSION_H_
#define CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_SESSION_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace metrics {

class CriticalUserJourney;

// Manages a single active instance of a Critical User Journey.
// Uses ui::InteractionSequence to track progress through steps.
class CriticalUserJourneySession {
 public:
  explicit CriticalUserJourneySession(const CriticalUserJourney* journey);
  ~CriticalUserJourneySession();

  CriticalUserJourneySession(const CriticalUserJourneySession&) = delete;
  CriticalUserJourneySession& operator=(const CriticalUserJourneySession&) =
      delete;

  // Starts the journey tracking in the given context.
  void Start(ui::ElementContext context);

  void set_on_done_callback(base::OnceClosure on_done_callback) {
    on_done_callback_ = std::move(on_done_callback);
  }

 private:
  // Build the underlying InteractionSequence::Builder from the journey
  // definition.
  ui::InteractionSequence::Builder BuildSequence(
      ui::ElementContext context,
      const CriticalUserJourney* journey,
      bool is_root);

  // Callbacks for InteractionSequence events.
  void OnStepStarted(int metric_id);
  void OnAborted(const ui::InteractionSequence::AbortedData& data);
  void OnCompleted();

  const raw_ptr<const CriticalUserJourney> journey_;
  base::OnceClosure on_done_callback_;
  std::unique_ptr<ui::InteractionSequence> sequence_;

  base::WeakPtrFactory<CriticalUserJourneySession> weak_factory_{this};
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_SESSION_H_
