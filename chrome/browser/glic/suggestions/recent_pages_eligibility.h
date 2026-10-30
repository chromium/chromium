// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SUGGESTIONS_RECENT_PAGES_ELIGIBILITY_H_
#define CHROME_BROWSER_GLIC_SUGGESTIONS_RECENT_PAGES_ELIGIBILITY_H_

class Profile;

namespace glic {

// Reasons why recent pages are ineligible to be used for zero-state
// suggestions.
enum class RecentPagesIneligibilityReason {
  kNone = 0,
  kHistorySyncOff = 1,
  kFreNotAccepted = 2,
  kTabContextDisabled = 3,
  kAgeRestricted = 4,
  // New values above this line.
  kMaxValue = kAgeRestricted,
};

// Returns whether recent pages can be used for zero-state suggestions,
// and if not, the ineligibility reason.
RecentPagesIneligibilityReason GetRecentPagesEligibility(Profile* profile);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SUGGESTIONS_RECENT_PAGES_ELIGIBILITY_H_
