// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_service.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_registry.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_session.h"
#include "chrome/browser/metrics/critical_user_journeys/features.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "ui/base/interaction/element_tracker.h"

namespace metrics {

CriticalUserJourneyService::CriticalUserJourneyService(Profile* profile)
    : profile_(profile) {}

CriticalUserJourneyService::~CriticalUserJourneyService() = default;

void CriticalUserJourneyService::Initialize() {
  if (!base::FeatureList::IsEnabled(kCriticalUserJourneyService)) {
    return;
  }

  RegisterJourneys(&registry_);

  for (const auto& journey : registry_.journeys()) {
    // Kill Switch: Skip registering triggers if the journey's feature flag is
    // disabled.
    if (!journey->IsEnabled()) {
      continue;
    }

    if (journey->steps().empty()) {
      continue;
    }

    const auto& trigger_step = journey->steps()[0];
    if (trigger_step->type == ui::InteractionSequence::StepType::kSubsequence) {
      for (const auto& branch : trigger_step->branches) {
        if (branch->steps().empty()) {
          continue;
        }
        RegisterJourneyTrigger(journey.get(), branch->steps()[0].get(),
                               branch->steps()[0]->metric_id);
      }
    } else {
      RegisterJourneyTrigger(journey.get(), trigger_step.get(), std::nullopt);
    }
  }
}

void CriticalUserJourneyService::RegisterJourneys(
    CriticalUserJourneyRegistry* registry) {
  registry->AddJourneys();
}

void CriticalUserJourneyService::RegisterJourneyTrigger(
    const CriticalUserJourney* journey,
    const CriticalUserJourneyStep* step,
    std::optional<int> metric_id) {
  ui::ElementIdentifier trigger_id = step->id;
  auto callback =
      base::BindRepeating(&CriticalUserJourneyService::OnJourneyStarted,
                          base::Unretained(this), journey, metric_id);

  switch (step->type) {
    case ui::InteractionSequence::StepType::kShown:
      subscriptions_.push_back(ui::ElementTracker::GetElementTracker()
                                   ->AddElementShownInAnyContextCallback(
                                       trigger_id, std::move(callback)));
      break;
    case ui::InteractionSequence::StepType::kActivated:
      subscriptions_.push_back(ui::ElementTracker::GetElementTracker()
                                   ->AddElementActivatedInAnyContextCallback(
                                       trigger_id, std::move(callback)));
      break;
    case ui::InteractionSequence::StepType::kCustomEvent:
      subscriptions_.push_back(
          ui::ElementTracker::GetElementTracker()
              ->AddCustomEventInAnyContextCallback(step->custom_event_type,
                                                   std::move(callback)));
      break;
    case ui::InteractionSequence::StepType::kHidden:
    case ui::InteractionSequence::StepType::kSubsequence:
      NOTREACHED();
  }
}

void CriticalUserJourneyService::OnJourneyStarted(
    const CriticalUserJourney* journey,
    std::optional<int> metric_id,
    ui::TrackedElement* element) {
  // 1. Find and preempt any existing session for this journey.
  auto it = std::find_if(
      active_sessions_.begin(), active_sessions_.end(),
      [journey](const auto& s) { return s->journey() == journey; });

  if (it != active_sessions_.end()) {
    // Safely extract the session before erasing it from the vector.
    // If we erase it directly, the session's destructor will synchronously
    // trigger `OnJourneyEnded`, which also attempts to modify
    // `active_sessions_`. Mutating the vector while it is already in the middle
    // of `erase()` causes a re-entrancy crash.
    std::unique_ptr<CriticalUserJourneySession> old_session = std::move(*it);
    active_sessions_.erase(it);
    // old_session goes out of scope and is destroyed cleanly.
  }

  // 2. Start the new session.
  auto session = std::make_unique<CriticalUserJourneySession>(journey);
  auto* session_ptr = session.get();
  session->set_on_done_callback(
      base::BindOnce(&CriticalUserJourneyService::OnJourneyEnded,
                     base::Unretained(this), base::Unretained(session_ptr)));

  active_sessions_.push_back(std::move(session));
  session_ptr->Start(metric_id, element);
}

void CriticalUserJourneyService::OnJourneyEnded(
    CriticalUserJourneySession* session,
    CriticalUserJourneySession::JourneyResult result) {
  if (result == CriticalUserJourneySession::JourneyResult::kCompleted &&
      session->journey()->hats_params().has_value()) {
    const auto& params = *session->journey()->hats_params();
    if (auto* hats_service = HatsServiceFactory::GetForProfile(
            profile_, /*create_if_necessary=*/true)) {
      base::OnceClosure success_callback =
          params.success_callback ? params.success_callback : base::DoNothing();
      base::OnceClosure failure_callback =
          params.failure_callback ? params.failure_callback : base::DoNothing();

      hats_service->LaunchSurvey(
          params.trigger, std::move(success_callback),
          std::move(failure_callback), params.product_specific_bits_data,
          params.product_specific_string_data, params.supplied_trigger_id,
          HatsService::SurveyOptions());
    }
  }

  auto it =
      std::find_if(active_sessions_.begin(), active_sessions_.end(),
                   [session](const auto& s) { return s.get() == session; });
  if (it != active_sessions_.end()) {
    active_sessions_.erase(it);
  }
}

}  // namespace metrics
