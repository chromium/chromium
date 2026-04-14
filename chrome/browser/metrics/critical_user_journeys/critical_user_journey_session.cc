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
                 base::TimeDelta timeout, ui::InteractionSequence*,
                 ui::TrackedElement*) {
                if (self) {
                  self->OnStepStarted(metric_id, timeout);
                }
              },
              weak_factory_.GetWeakPtr(),
              first_step_metric_id.value_or(step->metric_id),
              step->time_out_duration)));
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
                             weak_factory_.GetWeakPtr(), step->metric_id,
                             step->time_out_duration));

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

void CriticalUserJourneySession::OnStepStarted(int metric_id,
                                               base::TimeDelta timeout) {
  base::TimeTicks now = base::TimeTicks::Now();

  if (last_reached_metric_id_ == kNoMetricId) {
    journey_start_time_ = now;
  } else {
    base::TimeDelta step_duration = now - last_step_time_;
    std::string histogram_name =
        base::StrCat({"CriticalUserJourney.", journey_->name(), ".Step",
                      base::NumberToString(last_reached_metric_id_), "ToStep",
                      base::NumberToString(metric_id), "Duration"});
    base::UmaHistogramMediumTimes(histogram_name, step_duration);
  }

  last_step_time_ = now;
  last_reached_metric_id_ = metric_id;
  base::UmaHistogramSparse(
      base::StrCat({"CriticalUserJourney.", journey_->name(), ".StepReached"}),
      metric_id);

  if (!timeout.is_zero()) {
    timeout_timer_.Start(FROM_HERE, timeout, this,
                         &CriticalUserJourneySession::OnTimeout);
  } else {
    timeout_timer_.Stop();
  }
}

void CriticalUserJourneySession::OnTimeout() {
  was_timeout_ = true;
  sequence_.reset();
}

void CriticalUserJourneySession::OnAborted(
    const ui::InteractionSequence::AbortedData& data) {
  timeout_timer_.Stop();
  int aborted_metric_id = last_reached_metric_id_;
  if (!data.step_description.empty()) {
    CHECK_NE(aborted_metric_id, kNoMetricId);
    base::StringToInt(data.step_description, &aborted_metric_id);
  }

  base::UmaHistogramSparse(
      base::StrCat({"CriticalUserJourney.", journey_->name(), ".StepAborted"}),
      aborted_metric_id);

  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalUserJourney.", journey_->name(), ".Result"}),
      was_timeout_ ? JourneyResult::kTimeout : JourneyResult::kAborted);

  if (on_done_callback_) {
    std::move(on_done_callback_)
        .Run(was_timeout_ ? JourneyResult::kTimeout : JourneyResult::kAborted);
  }
}

void CriticalUserJourneySession::OnCompleted() {
  timeout_timer_.Stop();

  // Record overall duration
  if (!journey_start_time_.is_null()) {
    base::TimeDelta overall_duration =
        base::TimeTicks::Now() - journey_start_time_;
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {"CriticalUserJourney.", journey_->name(), ".OverallDuration"}),
        overall_duration);
  }

  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalUserJourney.", journey_->name(), ".Result"}),
      JourneyResult::kCompleted);

  if (journey_->completion_callback()) {
    journey_->completion_callback().Run();
  }
  if (on_done_callback_) {
    std::move(on_done_callback_).Run(JourneyResult::kCompleted);
  }
}

}  // namespace metrics
