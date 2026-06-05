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
  // Tab context requests when the panel is hidden are now reported as both
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

// LINT.IfChange(GeminiNavigationCaptureResult)
enum class GeminiNavigationCaptureResult {
  kSuccess = 0,
  kInvalidUrl = 1,
  kNonHttpsScheme = 2,
  kCIDTooLong = 3,
  kTargetUrlTooLong = 4,
  kNoTargetUrl = 5,
  kTurnIdTooLong = 6,
  kMaxValue = kTurnIdTooLong,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GeminiNavigationCaptureResult)

// LINT.IfChange(CannotActReason)
enum class CannotActReason {
  // Browser can actuate.
  kNone = 0,
  // The enterprise policy disables the actuation feature. Only applicable to
  // managed clients (Profile level, browser level or machine level).
  kDisabledByPolicy = 1,
  // The account is not eligible for the actuation.
  kAccountCapabilityIneligible = 2,
  // The account is not subscribed to one of the required AI subscription
  // tiers.
  kAccountMissingChromeBenefits = 3,
  // An enterprise account is logged in but there is no management to deliver
  // the policy. Actuation is disabled because the policy pref default value
  // is disabled.
  kEnterpriseWithoutManagement = 4,
  kMaxValue = kEnterpriseWithoutManagement,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorTaskCreateFailedReason)

// LINT.IfChange(GlicZoomAction)
enum class GlicZoomAction {
  kZoomIn = 0,
  kZoomOut = 1,
  kReset = 2,
  kMaxValue = kReset,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicZoomAction)

// LINT.IfChange(GlicProcessCounterAbuseVerdictResult)
enum class GlicProcessCounterAbuseVerdictResult {
  kSuccess = 0,
  kInvalidVerdict = 1,
  kNoInterstitialRequested = 2,
  kUrlMismatch = 3,
  kUnsupportedThreatType = 4,
  kInterstitialSkippedAllowlist = 5,
  kMaxValue = kInterstitialSkippedAllowlist,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicProcessCounterAbuseVerdictResult)

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_ENUMS_H_
