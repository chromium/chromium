// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"

#include <utility>

#include "base/check.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace metrics {

Branch::Branch(ui::ElementIdentifier id,
               ui::InteractionSequence::StepType type,
               int metric_id)
    : id(id), type(type), metric_id(metric_id) {}

Branch::Branch(ui::CustomElementEventType event_type, int metric_id)
    : type(ui::InteractionSequence::StepType::kCustomEvent),
      custom_event_type(event_type),
      metric_id(metric_id) {}

Branch::~Branch() = default;

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
    int metric_id) {
  auto step = std::make_unique<CriticalUserJourneyStep>();
  step->metric_id = metric_id;
  step->type = type;
  step->time_out_duration = base::Minutes(2);

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
  for (const auto& branch : branches) {
    CriticalUserJourney::Builder cuj_builder =
        CriticalUserJourney::Builder(nullptr);
    if (branch.id) {
      cuj_builder.AddStep(branch.id, branch.type, branch.metric_id);
    } else {
      cuj_builder.AddStep(branch.custom_event_type, branch.type,
                          branch.metric_id);
    }
    step->branches.push_back(cuj_builder.Build());
  }
  steps_.push_back(std::move(step));
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
