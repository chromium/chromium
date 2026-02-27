// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_ENUMS_H_
#define CHROME_BROWSER_GLIC_GLIC_ENUMS_H_

namespace glic {

// Add here Glic enums that should be visible to external code. If the enum is
// also used in Mojo, it should be defined in ./glic.mojom instead (also visible
// to external code).

// Error types for when attempting to extract context from a tab.
// LINT.IfChange(GlicGetContextFromTabError)
enum class GlicGetContextFromTabError {
  kUnknown = 0,
  // Tab context requests when the panel is hidden are now reported as both as
  // "hidden" and "error" in Glic.Api.* histograms.
  kPermissionDeniedWindowNotShowing_DEPRECATED = 1,
  kTabNotFound = 2,
  kPermissionDeniedContextPermissionNotEnabled = 3,
  kPermissionDenied = 4,
  kWebContentsChanged = 5,
  kPageContextNotEligible = 6,
  kMaxValue = kPageContextNotEligible,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicGetContextFromTabError)

// Represents the result of country or locale filtering.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GlicFilteringResult)
enum class GlicFilteringResult {
  kAllowedFilteringDisabled = 0,
  kBlockedInExclusionList = 1,
  kAllowedWildcardInclusion = 2,
  kAllowedInInclusionList = 3,
  kBlockedNotInInclusionList = 4,
  kMaxValue = kBlockedNotInInclusionList,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicFilteringResult)

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_ENUMS_H_
