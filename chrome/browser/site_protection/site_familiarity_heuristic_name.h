// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_HEURISTIC_NAME_H_
#define CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_HEURISTIC_NAME_H_

namespace site_protection {

// Site familiarity heuristics for the current page.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SiteFamiliarityHeuristicName {
  kNoHeuristicMatch = 0,
  kGlobalAllowlistMatch = 1,
  kVisitedMoreThanADayAgo = 2,
  kVisitedMoreThanFourHoursAgo = 3,
  kSiteEngagementScoreGte50 = 4,
  kSiteEngagementScoreGte25 = 5,
  kSiteEngagementScoreGte10 = 6,
  kSiteEngagementScoreExists = 7,
  kNoVisitsToAnySiteMoreThanADayAgo = 8,
  kGlobalAllowlistNotReady = 9,
  kFamiliarLikelyPreviouslyUnfamiliar = 10,
  kMaxValue = kFamiliarLikelyPreviouslyUnfamiliar,
};

// Subset of SiteFamiliarityHeuristicName for heuristics related to navigation
// history.
enum class SiteFamiliarityHistoryHeuristicName {
  kNoHeuristicMatch = 0,
  kVisitedMoreThanADayAgo = 1,
  kVisitedMoreThanFourHoursAgo = 2,
  kNoVisitsToAnySiteMoreThanADayAgo = 3,
  kVisitedMoreThanADayAgoPreviouslyUnfamiliar = 4,
};

}  // namespace site_protection

#endif  // CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_HEURISTIC_NAME_H_
