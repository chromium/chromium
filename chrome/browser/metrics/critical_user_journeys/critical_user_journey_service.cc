// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_service.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_registry.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_session.h"
#include "ui/base/interaction/element_tracker.h"

namespace metrics {

CriticalUserJourneyService::CriticalUserJourneyService(Profile* profile)
    : profile_(profile),
      registry_(std::make_unique<CriticalUserJourneyRegistry>()) {
  RegisterJourneys(registry_.get());

  for (const auto& journey : registry_->journeys()) {
    // We assume the first step's ID is the trigger for the journey.
    if (!journey->steps().empty()) {
      ui::ElementIdentifier trigger_id = journey->steps()[0]->id;
      subscriptions_.push_back(
          ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
              trigger_id, ui::ElementContext(),
              base::BindRepeating(&CriticalUserJourneyService::OnJourneyStarted,
                                  base::Unretained(this), journey.get())));
    }
  }
}

CriticalUserJourneyService::~CriticalUserJourneyService() = default;

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
  session_ptr->Start(element->context());
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
