// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_session.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_step.h"
#include "ui/base/interaction/element_tracker.h"

namespace {
// Defines the maximum number of steps in a journey.
constexpr int MAX_JOURNEY_STEPS = 20;
}  // namespace

namespace metrics {

CriticalUserJourneySession::CriticalUserJourneySession(
    const CriticalUserJourney* journey)
    : journey_(journey) {}

CriticalUserJourneySession::~CriticalUserJourneySession() = default;

void CriticalUserJourneySession::Start(ui::TrackedElement* element) {
  if (journey_->steps().empty()) {
    return;
  }

  sequence_ =
      BuildSequence(element->context(), journey_, /*is_root=*/true, element)
          .Build();
  sequence_->Start();
}

ui::InteractionSequence::Builder CriticalUserJourneySession::BuildSequence(
    ui::ElementContext context,
    const CriticalUserJourney* journey,
    bool is_root,
    ui::TrackedElement* initial_element) {
  ui::InteractionSequence::Builder builder;
  builder.SetContext(context);

  for (size_t i = 0; i < journey->steps().size(); ++i) {
    const auto& step = journey->steps()[i];
    if (i == 0 && initial_element) {
      // If we have an initial element, use it to start the sequence.
      builder.AddStep(ui::InteractionSequence::WithInitialElement(
          initial_element,
          base::BindOnce(
              [](base::WeakPtr<CriticalUserJourneySession> self, int metric_id,
                 ui::InteractionSequence*, ui::TrackedElement*) {
                if (self) {
                  self->OnStepStarted(metric_id);
                }
              },
              weak_factory_.GetWeakPtr(), step->metric_id)));
      continue;
    }

    if (step->branches.empty()) {
      builder.AddStep(ui::InteractionSequence::StepBuilder()
                          .SetElement(step->id)
                          .SetType(step->type)
                          .SetDescription(base::NumberToString(step->metric_id))
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
  last_reached_metric_id_ = metric_id;
  // Use ExactLinear to treat this as an enumerated histogram in the dashboard.
  // 100 is a reasonable maximum for the number of steps in a CUJ.
  base::UmaHistogramExactLinear(
      base::StrCat({"CriticalUserJourney.", journey_->name(), ".StepReached"}),
      metric_id, MAX_JOURNEY_STEPS);
}

void CriticalUserJourneySession::OnAborted(
    const ui::InteractionSequence::AbortedData& data) {
  int aborted_metric_id = last_reached_metric_id_;
  if (!data.step_description.empty()) {
    CHECK_NE(aborted_metric_id, -1);
    base::StringToInt(data.step_description, &aborted_metric_id);
  }
  base::UmaHistogramExactLinear(
      base::StrCat(
          {"CriticalUserJourney.", journey_->name(), ".AbortedAtStep"}),
      aborted_metric_id, MAX_JOURNEY_STEPS);
  if (on_done_callback_) {
    std::move(on_done_callback_).Run();
  }
}

void CriticalUserJourneySession::OnCompleted() {
  base::UmaHistogramBoolean(
      base::StrCat({"CriticalUserJourney.", journey_->name(), ".Completed"}),
      true);
  if (journey_->completion_callback()) {
    journey_->completion_callback().Run();
  }
  if (on_done_callback_) {
    std::move(on_done_callback_).Run();
  }
}

}  // namespace metrics
