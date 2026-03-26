// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_session.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_step.h"
#include "ui/base/interaction/element_tracker.h"

namespace metrics {

CriticalUserJourneySession::CriticalUserJourneySession(
    const CriticalUserJourney* journey)
    : journey_(journey) {}

CriticalUserJourneySession::~CriticalUserJourneySession() = default;

void CriticalUserJourneySession::Start(std::optional<int> first_step_metric_id,
                                       ui::TrackedElement* element) {
  if (journey_->steps().empty() || !element) {
    return;
  }

  sequence_ = BuildSequence(element->context(), journey_, /*is_root=*/true,
                            first_step_metric_id, element)
                  .Build();
  sequence_->Start();
}

ui::InteractionSequence::Builder CriticalUserJourneySession::BuildSequence(
    ui::ElementContext context,
    const CriticalUserJourney* journey,
    bool is_root,
    std::optional<int> first_step_metric_id,
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
              weak_factory_.GetWeakPtr(),
              first_step_metric_id.value_or(step->metric_id))));
      continue;
    }

    if (step->branches.empty()) {
      auto step_builder = ui::InteractionSequence::StepBuilder();
      if (step->type == ui::InteractionSequence::StepType::kCustomEvent) {
        step_builder.SetType(step->type, step->custom_event_type);
      } else {
        step_builder.SetType(step->type);
        step_builder.SetElement(step->id);
      }
      step_builder.SetContext(step->context)
          .SetDescription(base::NumberToString(step->metric_id))
          .SetStartCallback(
              base::BindOnce(&CriticalUserJourneySession::OnStepStarted,
                             weak_factory_.GetWeakPtr(), step->metric_id));

      builder.AddStep(std::move(step_builder));
    } else {
      ui::InteractionSequence::StepBuilder step_builder =
          std::move(ui::InteractionSequence::StepBuilder().SetSubsequenceMode(
              step->mode));
      for (const auto& branch : step->branches) {
        // Recursively add subsequences for each branch.
        step_builder.AddSubsequence(BuildSequence(
            context, branch.get(), /*is_root=*/false, std::nullopt, nullptr));
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
  base::UmaHistogramSparse(
      base::StrCat({"CriticalUserJourney.", journey_->name(), ".StepReached"}),
      metric_id);
}

void CriticalUserJourneySession::OnAborted(
    const ui::InteractionSequence::AbortedData& data) {
  int aborted_metric_id = last_reached_metric_id_;
  if (!data.step_description.empty()) {
    CHECK_NE(aborted_metric_id, -1);
    base::StringToInt(data.step_description, &aborted_metric_id);
  }

  base::UmaHistogramSparse(
      base::StrCat({"CriticalUserJourney.", journey_->name(), ".StepAborted"}),
      aborted_metric_id);

  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalUserJourney.", journey_->name(), ".Result"}),
      JourneyResult::kAborted);

  if (on_done_callback_) {
    std::move(on_done_callback_).Run();
  }
}

void CriticalUserJourneySession::OnCompleted() {
  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalUserJourney.", journey_->name(), ".Result"}),
      JourneyResult::kCompleted);

  if (journey_->completion_callback()) {
    journey_->completion_callback().Run();
  }
  if (on_done_callback_) {
    std::move(on_done_callback_).Run();
  }
}

}  // namespace metrics
