// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_session.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_step.h"

namespace metrics {

CriticalUserJourneySession::CriticalUserJourneySession(
    const CriticalUserJourney* journey)
    : journey_(journey) {}

CriticalUserJourneySession::~CriticalUserJourneySession() = default;

void CriticalUserJourneySession::Start(ui::ElementContext context) {
  sequence_ = BuildSequence(context, journey_, /*is_root=*/true).Build();
  sequence_->Start();
}

ui::InteractionSequence::Builder CriticalUserJourneySession::BuildSequence(
    ui::ElementContext context,
    const CriticalUserJourney* journey,
    bool is_root) {
  ui::InteractionSequence::Builder builder;
  builder.SetContext(context);

  for (const auto& step : journey->steps()) {
    if (step->branches.empty()) {
      builder.AddStep(ui::InteractionSequence::StepBuilder()
                          .SetElement(step->id)
                          .SetType(step->type)
                          .SetStartCallback(base::BindOnce(
                              &CriticalUserJourneySession::OnStepStarted,
                              weak_factory_.GetWeakPtr(), step->metric_id)));
    } else {
      ui::InteractionSequence::StepBuilder step_builder =
          std::move(ui::InteractionSequence::StepBuilder().SetSubsequenceMode(
              step->mode));
      for (const auto& branch : step->branches) {
        // Recursively add subsequences for each branch.
        step_builder.AddSubsequence(
            BuildSequence(context, branch.get(), /*is_root=*/false));
      }
      builder.AddStep(std::move(step_builder));
    }
  }

  if (is_root) {
    builder.SetAbortedCallback(base::BindOnce(
        &CriticalUserJourneySession::OnAborted, weak_factory_.GetWeakPtr()));
    builder.SetCompletedCallback(base::BindOnce(
        &CriticalUserJourneySession::OnCompleted, weak_factory_.GetWeakPtr()));
  }

  return builder;
}

void CriticalUserJourneySession::OnStepStarted(int metric_id) {
  // TODO(crbug.com/488075669): Record metric.
}

void CriticalUserJourneySession::OnAborted(
    const ui::InteractionSequence::AbortedData& data) {
  // TODO(crbug.com/488075669): Record metric.
  std::move(on_done_callback_).Run();
}

void CriticalUserJourneySession::OnCompleted() {
  // TODO(crbug.com/488075669): Record metric.
  if (journey_->completion_callback()) {
    journey_->completion_callback().Run();
  }
  std::move(on_done_callback_).Run();
}

}  // namespace metrics
