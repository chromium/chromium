// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_registry.h"

#include <utility>

namespace metrics {

CriticalUserJourneyRegistry::CriticalUserJourneyRegistry() = default;
CriticalUserJourneyRegistry::~CriticalUserJourneyRegistry() = default;

void CriticalUserJourneyRegistry::AddJourney(
    std::unique_ptr<CriticalUserJourney> journey) {
  journeys_.push_back(std::move(journey));
}

}  // namespace metrics
