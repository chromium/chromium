// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_FEATURES_H_
#define CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_FEATURES_H_

#include "base/feature_list.h"

namespace metrics {

BASE_DECLARE_FEATURE(kCriticalUserJourneyService);

// Dedicated feature flags for each journey.
BASE_DECLARE_FEATURE(kClearBrowsingHistoryJourney);
BASE_DECLARE_FEATURE(kViewDownloadedFileJourney);
BASE_DECLARE_FEATURE(kViewDownloadedFileFromAppMenuJourney);
BASE_DECLARE_FEATURE(kPinExtensionJourney);

// HaTS Survey feature flags.
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDownloadJourney);
extern const char kHatsSurveyTriggerDownloadJourney[];

BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForPinExtensionJourney);
extern const char kHatsSurveyTriggerPinExtensionJourney[];

BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForClearBrowsingHistory);
extern const char kHatsSurveyTriggerClearBrowsingHistory[];

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_FEATURES_H_
