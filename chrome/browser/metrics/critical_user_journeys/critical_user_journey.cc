// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"

#include <utility>

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

CriticalUserJourney::Builder::Builder(std::string name)
    : name_(std::move(name)) {}
CriticalUserJourney::Builder::~Builder() = default;

CriticalUserJourney::Builder& CriticalUserJourney::Builder::AddStep(
    ui::ElementIdentifier id,
    ui::InteractionSequence::StepType type,
    int metric_id) {
  auto step = std::make_unique<CriticalUserJourneyStep>();
  step->id = id;
  step->type = type;
  step->metric_id = metric_id;
  steps_.push_back(std::move(step));
  return *this;
}

CriticalUserJourney::Builder& CriticalUserJourney::Builder::AddAnyOf(
    const std::vector<Branch>& branches) {
  auto step = std::make_unique<CriticalUserJourneyStep>();
  step->type = ui::InteractionSequence::StepType::kSubsequence;
  step->mode = ui::InteractionSequence::SubsequenceMode::kAtLeastOne;
  for (const auto& branch : branches) {
    auto branch_journey = CriticalUserJourney::Builder("")
                              .AddStep(branch.id, branch.type, branch.metric_id)
                              .Build();
    step->branches.push_back(std::move(branch_journey));
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

std::unique_ptr<CriticalUserJourney> CriticalUserJourney::Builder::Build() {
  return std::make_unique<CriticalUserJourney>(
      std::move(name_), std::move(steps_), std::move(completion_callback_));
}

CriticalUserJourney::CriticalUserJourney(
    std::string name,
    std::vector<std::unique_ptr<CriticalUserJourneyStep>> steps,
    base::RepeatingClosure completion_callback)
    : name_(std::move(name)),
      steps_(std::move(steps)),
      completion_callback_(std::move(completion_callback)) {}

CriticalUserJourney::~CriticalUserJourney() = default;

}  // namespace metrics
