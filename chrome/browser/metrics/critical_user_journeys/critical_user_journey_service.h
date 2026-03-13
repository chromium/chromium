// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_SERVICE_H_
#define CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ui {
class TrackedElement;
}

namespace metrics {

class CriticalUserJourney;
class CriticalUserJourneyRegistry;
class CriticalUserJourneySession;

// Service responsible for tracking and managing active Critical User Journeys.
// Listens for journey start triggers and manages the lifecycle of journey
// sessions.
class CriticalUserJourneyService : public KeyedService {
 public:
  explicit CriticalUserJourneyService(Profile* profile);
  ~CriticalUserJourneyService() override;

  CriticalUserJourneyService(const CriticalUserJourneyService&) = delete;
  CriticalUserJourneyService& operator=(const CriticalUserJourneyService&) =
      delete;

 protected:
  // Registers journeys with the registry. Overridden by platform-specific
  // implementations.
  virtual void RegisterJourneys(CriticalUserJourneyRegistry* registry);

 private:
  void OnJourneyStarted(const CriticalUserJourney* journey,
                        ui::TrackedElement* element);
  void OnJourneyEnded(CriticalUserJourneySession* session);

  const raw_ptr<Profile> profile_;
  std::unique_ptr<CriticalUserJourneyRegistry> registry_;
  std::vector<std::unique_ptr<CriticalUserJourneySession>> active_sessions_;
  std::vector<base::CallbackListSubscription> subscriptions_;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_SERVICE_H_
