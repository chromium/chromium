// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/time/time.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace metrics {

std::string GetCujDurationBucket(base::TimeDelta duration) {
  if (duration < base::Seconds(2)) {
    return "<2s";
  }
  if (duration < base::Seconds(5)) {
    return "2s-5s";
  }
  if (duration < base::Seconds(15)) {
    return "5s-15s";
  }
  if (duration < base::Minutes(1)) {
    return "15s-60s";
  }
  return ">60s";
}

Branch::Branch(ui::ElementIdentifier id,
               ui::InteractionSequence::StepType type,
               int metric_id,
               ui::InteractionSequence::ContextMode context_mode)
    : id(id), type(type), metric_id(metric_id), context_mode(context_mode) {}

Branch::Branch(ui::CustomElementEventType event_type,
               int metric_id,
               ui::InteractionSequence::ContextMode context_mode)
    : type(ui::InteractionSequence::StepType::kCustomEvent),
      custom_event_type(event_type),
      metric_id(metric_id),
      context_mode(context_mode) {}

Branch::~Branch() = default;
Branch::Branch(const Branch&) = default;
Branch& Branch::operator=(const Branch&) = default;
Branch::Branch(Branch&&) noexcept = default;
Branch& Branch::operator=(Branch&&) noexcept = default;

HatsParams::HatsParams() = default;
HatsParams::HatsParams(const HatsParams&) = default;
HatsParams& HatsParams::operator=(const HatsParams&) = default;
HatsParams::~HatsParams() = default;

CriticalUserJourney::Builder::Builder(const base::Feature* feature)
    : feature_(feature) {}
CriticalUserJourney::Builder::~Builder() = default;

CriticalUserJourney::Builder& CriticalUserJourney::Builder::AddStep(
    std::variant<ui::ElementIdentifier, ui::CustomElementEventType> event,
    ui::InteractionSequence::StepType type,
    int metric_id,
    ui::InteractionSequence::ContextMode context_mode) {
  auto step = std::make_unique<CriticalUserJourneyStep>();
  step->metric_id = metric_id;
  step->type = type;
  step->time_out_duration = kDefaultCujStepTimeout;
  step->context = context_mode;
  if (context_mode == ui::InteractionSequence::ContextMode::kAny) {
    step->in_any_context = true;
  }

  if (std::holds_alternative<ui::CustomElementEventType>(event)) {
    CHECK_EQ(type, ui::InteractionSequence::StepType::kCustomEvent)
        << "Custom events implicitly require type kCustomEvent.";
    step->custom_event_type = std::get<ui::CustomElementEventType>(event);
  } else {
    CHECK_NE(type, ui::InteractionSequence::StepType::kCustomEvent)
        << "Element identifiers require a non-custom StepType.";
    step->id = std::get<ui::ElementIdentifier>(event);
  }

  steps_.push_back(std::move(step));
  return *this;
}

CriticalUserJourney::Builder& CriticalUserJourney::Builder::AddAnyOf(
    const std::vector<Branch>& branches) {
  auto step = std::make_unique<CriticalUserJourneyStep>();
  step->type = ui::InteractionSequence::StepType::kSubsequence;
  step->mode = ui::InteractionSequence::SubsequenceMode::kAtLeastOne;
  const bool has_exit_branch =
      std::any_of(branches.begin(), branches.end(),
                  [](const Branch& branch) { return branch.is_exit_branch; });
  for (const auto& branch : branches) {
    CriticalUserJourney::Builder cuj_builder =
        CriticalUserJourney::Builder(nullptr);
    if (branch.id) {
      cuj_builder.AddStep(branch.id, branch.type, branch.metric_id,
                          branch.context_mode);
    } else {
      cuj_builder.AddStep(branch.custom_event_type, branch.type,
                          branch.metric_id, branch.context_mode);
    }
    auto built_branch = cuj_builder.Build();
    CHECK(!built_branch->steps().empty());
    built_branch->steps()[0]->is_exit_branch = branch.is_exit_branch;
    built_branch->steps()[0]->min_dwell_duration = branch.min_dwell_duration;
    built_branch->steps()[0]->max_dwell_duration = branch.max_dwell_duration;
    built_branch->steps()[0]->in_exit_branch_group = has_exit_branch;
    built_branch->steps()[0]->time_out_duration = branch.time_out_duration;
    built_branch->steps()[0]->hats_entry_point = branch.hats_entry_point;
    step->branches.push_back(std::move(built_branch));
  }
  steps_.push_back(std::move(step));
  return *this;
}

CriticalUserJourney::Builder& CriticalUserJourney::Builder::SetStepTimeout(
    base::TimeDelta timeout) {
  CHECK_GT(timeout, base::TimeDelta());
  CHECK(!steps_.empty()) << "SetStepTimeout() requires a preceding step.";
  CriticalUserJourneyStep* step = steps_.back().get();
  if (step->branches.empty()) {
    step->time_out_duration = timeout;
    return *this;
  }
  for (const auto& branch : step->branches) {
    for (const auto& branch_step : branch->steps()) {
      branch_step->time_out_duration = timeout;
    }
  }
  return *this;
}

CriticalUserJourney::Builder&
CriticalUserJourney::Builder::AddCustomCompletionCallback(
    base::RepeatingClosure callback) {
  completion_callback_ = std::move(callback);
  return *this;
}

CriticalUserJourney::Builder&
CriticalUserJourney::Builder::LaunchHatsSurveyOnCompletion(HatsParams params) {
  hats_params_ = std::move(params);
  return *this;
}

std::unique_ptr<CriticalUserJourney> CriticalUserJourney::Builder::Build() {
  // Exit branches end the journey, but the underlying InteractionSequence
  // cannot be reset from within a step callback. Restrict them to the final
  // step so that no further steps can run after an exit branch is reached.
  // They are also not allowed in the first step, which acts as the trigger.
  for (size_t i = 0; i < steps_.size(); ++i) {
    const bool exit_allowed = i > 0 && i + 1 == steps_.size();
    for (const auto& branch : steps_[i]->branches) {
      for (const auto& branch_step : branch->steps()) {
        CHECK(exit_allowed || !branch_step->is_exit_branch)
            << "Exit branches are only allowed in the final step of a journey "
               "and not in the first step.";
      }
    }
  }

  return std::make_unique<CriticalUserJourney>(feature_, std::move(steps_),
                                               std::move(completion_callback_),
                                               std::move(hats_params_));
}

CriticalUserJourney::CriticalUserJourney(
    const base::Feature* feature,
    std::vector<std::unique_ptr<CriticalUserJourneyStep>> steps,
    base::RepeatingClosure completion_callback,
    std::optional<HatsParams> hats_params)
    : feature_(feature),
      steps_(std::move(steps)),
      completion_callback_(std::move(completion_callback)),
      hats_params_(std::move(hats_params)) {}

CriticalUserJourney::~CriticalUserJourney() = default;

}  // namespace metrics
