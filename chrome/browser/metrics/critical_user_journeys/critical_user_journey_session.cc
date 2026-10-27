// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_session.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
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

  ui::InteractionSequence::Builder builder =
      BuildSequence(element->context(), journey_, /*is_root=*/true,
                    first_step_metric_id, element);
  if (journey_->steps().size() == 1 &&
      last_reached_metric_id_ != kNoMetricId) {
    OnCompleted();
    return;
  }
  sequence_ = builder.Build();
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
      const CriticalUserJourneyStep* trigger_step = step.get();
      if (!step->branches.empty()) {
        // When Step 0 is an `AddAnyOf()` group, `step->type` is `kSubsequence`
        // and the trigger step metadata lives on the matching branch's first
        // step (matched by element ID, or by `first_step_metric_id` for custom
        // event triggers).
        for (const auto& branch : step->branches) {
          if (branch->steps().empty()) {
            continue;
          }
          const auto* branch_step = branch->steps()[0].get();
          const bool id_matches =
              branch_step->id &&
              branch_step->id == initial_element->identifier();
          const bool metric_matches =
              first_step_metric_id.has_value() &&
              branch_step->metric_id == *first_step_metric_id;
          if (id_matches &&
              (!first_step_metric_id.has_value() || metric_matches)) {
            trigger_step = branch_step;
            break;
          }
          if (!branch_step->id && metric_matches) {
            trigger_step = branch_step;
            break;
          }
        }
      }

      CHECK_NE(trigger_step->type,
               ui::InteractionSequence::StepType::kSubsequence);
      if (trigger_step->id) {
        CHECK_EQ(initial_element->identifier(), trigger_step->id);
      }

      const int metric_id =
          first_step_metric_id.value_or(trigger_step->metric_id);
      if (trigger_step->type == ui::InteractionSequence::StepType::kShown) {
        builder.AddStep(ui::InteractionSequence::WithInitialElement(
            initial_element,
            base::BindOnce(
                [](base::WeakPtr<CriticalUserJourneySession> self,
                   int metric_id, base::TimeDelta timeout, bool is_exit_branch,
                   base::TimeDelta min_dwell_duration, ui::InteractionSequence*,
                   ui::TrackedElement*) {
                  if (self) {
                    self->OnStepStarted(metric_id, timeout, is_exit_branch,
                                        min_dwell_duration);
                  }
                },
                weak_factory_.GetWeakPtr(), metric_id,
                trigger_step->time_out_duration, trigger_step->is_exit_branch,
                trigger_step->min_dwell_duration)));
      } else {
        // `CriticalUserJourneyService::RegisterJourneyTrigger()` subscribes to
        // Step 0's trigger event (`kActivated` or `kCustomEvent`) via
        // `ElementTracker` and starts the session only after that event has
        // already fired on `initial_element`. Do not use
        // `WithInitialElement()` here (which constructs a `StepType::kShown`
        // step and would overwrite the declared step type); record Step 0
        // directly and build the `InteractionSequence` from Step 1 onward.
        OnStepStarted(metric_id, trigger_step->time_out_duration,
                      trigger_step->is_exit_branch,
                      trigger_step->min_dwell_duration);
      }
      continue;
    }

    if (step->branches.empty()) {
      auto step_builder = ui::InteractionSequence::StepBuilder();
      if (step->type == ui::InteractionSequence::StepType::kCustomEvent) {
        step_builder.SetType(step->type, step->custom_event_type);
      } else {
        step_builder.SetType(step->type);
        step_builder.SetElement(step->id);
        if (step->type == ui::InteractionSequence::StepType::kHidden) {
          step_builder.SetTransitionOnlyOnEvent(true);
        }
      }
      step_builder.SetContext(step->context)
          .SetDescription(base::NumberToString(step->metric_id))
          .SetStartCallback(
              base::BindOnce(&CriticalUserJourneySession::OnStepStarted,
                             weak_factory_.GetWeakPtr(), step->metric_id,
                             step->time_out_duration, step->is_exit_branch,
                             step->min_dwell_duration));
      if (step->in_exit_branch_group) {
        // All branches of an AnyOf start at the same time. When one of them
        // is an exit branch, the completion branch's element (e.g. a button
        // inside a dialog) may not be visible yet. Without this, that branch
        // would abort immediately and only the exit branch could ever finish.
        // Other steps keep the InteractionSequence defaults so existing
        // journeys still abort when their element is missing.
        step_builder.SetMustBeVisibleAtStart(false);
      }

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

void CriticalUserJourneySession::OnStepStarted(
    int metric_id,
    base::TimeDelta timeout,
    bool is_exit_branch,
    base::TimeDelta min_dwell_duration) {
  base::TimeTicks now = base::TimeTicks::Now();

  if (last_reached_metric_id_ == kNoMetricId) {
    journey_start_time_ = now;
  } else {
    base::TimeDelta step_duration = now - last_step_time_;
    if (is_exit_branch && !min_dwell_duration.is_zero() &&
        step_duration < min_dwell_duration) {
      aborted_by_dwell_guardrail_ = true;
      return;
    }
    std::string histogram_name =
        base::StrCat({"CriticalUserJourney.", journey_->name(), ".Step",
                      base::NumberToString(last_reached_metric_id_), "ToStep",
                      base::NumberToString(metric_id), "Duration"});
    base::UmaHistogramMediumTimes(histogram_name, step_duration);
  }

  if (is_exit_branch) {
    completed_via_exit_branch_ = true;
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

  const std::string result_histogram =
      base::StrCat({"CriticalUserJourney.", journey_->name(), ".Result"});

  if (aborted_by_dwell_guardrail_) {
    // The exit branch was reached too quickly to count as a deliberate
    // abandonment. The InteractionSequence still completes because the exit
    // subsequence finished, so the result is reported here.
    base::UmaHistogramSparse(base::StrCat({"CriticalUserJourney.",
                                           journey_->name(), ".StepAborted"}),
                             last_reached_metric_id_);
    base::UmaHistogramEnumeration(result_histogram,
                                  JourneyResult::kAbandonedBelowDwell);
    if (on_done_callback_) {
      std::move(on_done_callback_).Run(JourneyResult::kAbandonedBelowDwell);
    }
    return;
  }

  if (completed_via_exit_branch_) {
    // OverallDuration and the completion callback only describe successful
    // journeys, so they are skipped for abandoned ones. `on_done_callback_`
    // still runs so the service can launch the HaTS survey.
    base::UmaHistogramEnumeration(result_histogram, JourneyResult::kAbandoned);
    if (on_done_callback_) {
      std::move(on_done_callback_).Run(JourneyResult::kAbandoned);
    }
    return;
  }

  // Record overall duration
  if (!journey_start_time_.is_null()) {
    base::TimeDelta overall_duration =
        base::TimeTicks::Now() - journey_start_time_;
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {"CriticalUserJourney.", journey_->name(), ".OverallDuration"}),
        overall_duration);
  }

  base::UmaHistogramEnumeration(result_histogram, JourneyResult::kCompleted);

  if (journey_->completion_callback()) {
    journey_->completion_callback().Run();
  }
  if (on_done_callback_) {
    std::move(on_done_callback_).Run(JourneyResult::kCompleted);
  }
}

}  // namespace metrics
