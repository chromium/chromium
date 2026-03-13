// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_REGISTRY_H_
#define CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_REGISTRY_H_

#include <map>
#include <memory>
#include <vector>

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"
#include "ui/base/interaction/element_identifier.h"

namespace metrics {

// Registry for all defined Critical User Journeys.
// Used to store and retrieve journey definitions by their starting element.
class CriticalUserJourneyRegistry {
 public:
  CriticalUserJourneyRegistry();
  ~CriticalUserJourneyRegistry();

  CriticalUserJourneyRegistry(const CriticalUserJourneyRegistry&) = delete;
  CriticalUserJourneyRegistry& operator=(const CriticalUserJourneyRegistry&) =
      delete;

  // Adds a journey to the registry.
  void AddJourney(std::unique_ptr<CriticalUserJourney> journey);

  // Returns all registered journeys.
  const std::vector<std::unique_ptr<CriticalUserJourney>>& journeys() const {
    return journeys_;
  }

 private:
  std::vector<std::unique_ptr<CriticalUserJourney>> journeys_;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_REGISTRY_H_
