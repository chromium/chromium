// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_SERVICE_H_
#define CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_registry.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_session.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ui {
class TrackedElement;
}

namespace metrics {

class CriticalUserJourney;

// Service responsible for tracking and managing active Critical User Journeys.
// Listens for journey start triggers and manages the lifecycle of journey
// sessions.
class CriticalUserJourneyService : public KeyedService {
 public:
  // LINT.IfChange(CriticalUserJourneyHaTSEvent)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CriticalUserJourneyHaTSEvent {
    // The request to show the survey was sent to the HaTS service.
    kTriggered = 0,
    // The survey was successfully displayed to the user.
    kShown = 1,
    // The survey request was rejected or failed to show (e.g. due to rate
    // limiting or cooldown).
    kFailed = 2,
    kMaxValue = kFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/critical_user_journeys/enums.xml:CriticalUserJourneyHaTSEvent)

  explicit CriticalUserJourneyService(Profile* profile);
  ~CriticalUserJourneyService() override;

  CriticalUserJourneyService(const CriticalUserJourneyService&) = delete;
  CriticalUserJourneyService& operator=(const CriticalUserJourneyService&) =
      delete;

  // Initializes the service, registers journeys and sets up triggers.
  void Initialize();

 protected:
  // Registers journeys with the registry. Overridden by platform-specific
  // implementations.
  virtual void RegisterJourneys(CriticalUserJourneyRegistry* registry);

 private:
  // Helper function to subscribe the first step of a journey into
  // `subscriptions_` so it can be tracked.
  void RegisterJourneyTrigger(const CriticalUserJourney* journey,
                              const CriticalUserJourneyStep* step,
                              std::optional<int> metric_id);

  void OnJourneyStarted(const CriticalUserJourney* journey,
                        std::optional<int> metric_id,
                        ui::TrackedElement* element);
  void OnJourneyEnded(CriticalUserJourneySession* session,
                      CriticalUserJourneySession::JourneyResult result);

  void LogHaTSEventAndRunCallback(const std::string& journey_name,
                                  CriticalUserJourneyHaTSEvent event,
                                  base::RepeatingClosure callback);

  const raw_ptr<Profile> profile_;
  CriticalUserJourneyRegistry registry_;
  std::vector<std::unique_ptr<CriticalUserJourneySession>> active_sessions_;
  std::vector<base::CallbackListSubscription> subscriptions_;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_SERVICE_H_
