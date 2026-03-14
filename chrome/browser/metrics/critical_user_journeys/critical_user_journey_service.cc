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
    if (journey->steps().empty()) {
      continue;
    }

    const auto& trigger_step = journey->steps()[0];
    ui::ElementIdentifier trigger_id = trigger_step->id;
    auto callback =
        base::BindRepeating(&CriticalUserJourneyService::OnJourneyStarted,
                            base::Unretained(this), journey.get());

    switch (trigger_step->type) {
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
      case ui::InteractionSequence::StepType::kHidden:
      case ui::InteractionSequence::StepType::kCustomEvent:
      case ui::InteractionSequence::StepType::kSubsequence:
        NOTIMPLEMENTED();
        break;
    }
  }
}

void CriticalUserJourneyService::RegisterJourneys(
    CriticalUserJourneyRegistry* registry) {
  // TODO(crbug.com/488075669): Populate registry with journeys.
}

void CriticalUserJourneyService::OnJourneyStarted(
    const CriticalUserJourney* journey,
    ui::TrackedElement* element) {
  auto session = std::make_unique<CriticalUserJourneySession>(journey);
  auto* session_ptr = session.get();
  session->set_on_done_callback(
      base::BindOnce(&CriticalUserJourneyService::OnJourneyEnded,
                     base::Unretained(this), base::Unretained(session_ptr)));

  active_sessions_.push_back(std::move(session));
  session_ptr->Start(element);
}

void CriticalUserJourneyService::OnJourneyEnded(
    CriticalUserJourneySession* session) {
  auto it =
      std::find_if(active_sessions_.begin(), active_sessions_.end(),
                   [session](const auto& s) { return s.get() == session; });
  if (it != active_sessions_.end()) {
    active_sessions_.erase(it);
  }
}

}  // namespace metrics
