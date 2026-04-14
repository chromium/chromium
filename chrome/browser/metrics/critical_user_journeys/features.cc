// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/features.h"

#include "base/feature_list.h"

namespace metrics {

BASE_FEATURE(kCriticalUserJourneyService, base::FEATURE_DISABLED_BY_DEFAULT);

// These can be toggled by default as needed.
BASE_FEATURE(kClearBrowsingHistoryJourney, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kViewDownloadedFileJourney, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kViewDownloadedFileFromAppMenuJourney,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPinExtensionJourney, base::FEATURE_DISABLED_BY_DEFAULT);

// HaTS.
BASE_FEATURE(kHappinessTrackingSurveysForDownloadJourney,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kHappinessTrackingSurveysForPinExtensionJourney,
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kHatsSurveyTriggerDownloadJourney[] = "download-journey";
const char kHatsSurveyTriggerPinExtensionJourney[] = "pin-extension-journey";

BASE_FEATURE(kHappinessTrackingSurveysForClearBrowsingHistory,
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kHatsSurveyTriggerClearBrowsingHistory[] =
    "clear-browsing-history-journey";

}  // namespace metrics
