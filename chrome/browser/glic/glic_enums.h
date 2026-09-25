// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_ENUMS_H_
#define CHROME_BROWSER_GLIC_GLIC_ENUMS_H_

namespace glic {

// Add here Glic enums that should be visible to external code. If the enum is
// also used in Mojo, it should be defined in ./glic.mojom instead (also visible
// to external code).

// The source of a zoom action (e.g., keyboard shortcut, scroll).
enum class ZoomSource {
  kHotkey,
  kHotkeyWithShift,
  kScroll,
  kMaxValue = kScroll,
};

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
  kZoomInAtMax = 3,
  kZoomOutAtMin = 4,
  kMaxValue = kZoomOutAtMin,
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

enum class EmbedderType {
  kUnknown,
  kSidePanel,
  kFloaty,
  kTab,
  kMaxValue = kTab,
};

// Whether the Glic client is usable, as seen by the browser.
//
// This is derived by `Host` rather than reported to it. The client is ready
// exactly when `mojom::WebClientState` says it has come up, so `kReady` and
// the web client state machine cannot disagree in either direction. Whoever
// hosts the client -- chrome://glic, or the GlicNoWebview overlay -- supplies
// only the one bit the browser cannot see for itself: whether a client that is
// not up yet is still loading or has failed. This lets browser-side code wait
// on the client without knowing which host is in use, or anything about its
// internal state machine.
//
// Responsiveness is a separate axis: a client that is up but unresponsive
// (`WebClientState::kUnresponsive`) is still `kReady` here, because it can
// become responsive again on its own.
enum class ClientLoadState {
  // The client is being created or is still loading. It cannot be used yet,
  // but may become ready later.
  kLoading,
  // The client has finished loading and can be used.
  kReady,
  // The client failed to load. It will not become ready again unless it is
  // reloaded.
  kError,
};

// Why the Glic client failed to load, i.e. failed to become usable. Scoped to
// failures that prevent the client from reaching a usable state; errors raised
// by a client that is already running are a separate concept.
//
// This describes the *cause* of a failure, not the panel that was shown. The
// same causes exist whether the client is hosted in a <webview> by
// chrome://glic or rendered directly by the GlicNoWebview overlay, so the
// browser can be the single source of truth for recording them in both worlds.
// The host page reports its cause over mojo as mojom::ClientLoadErrorReason
// and GlicPageHandler translates it into this enum; the overlay reports this
// enum directly. A cause with no webview analogue therefore needs no mojom
// change.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ClientLoadErrorReason)
enum class ClientLoadErrorReason {
  // The cause could not be determined.
  kUnknown = 0,

  // Eligibility gates. The client was never loaded because the profile is not
  // permitted to use Glic.

  // Glic is not available for this profile.
  kUnavailable = 1,

  // The signed-in account lacks the required capabilities.
  kIneligibleAccount = 2,

  // Glic is not available in the user's country.
  kLocationMismatch = 3,

  // Enterprise policy disables Glic. Covers both the plain and
  // learn-more-link variants of the panel, which differ only in presentation.
  kDisabledByAdmin = 4,

  // The browser has no network connection.
  kOffline = 5,

  // The user needs to sign in. Covers both the pre-load auth check and the
  // guest navigating to a login page.
  kSignIn = 6,

  // Resyncing cookies into the guest partition failed, so the client could not
  // be authenticated.
  kCookieSyncFailed = 7,

  // Guest failures. The guest contents itself did not come up.

  // The guest navigation committed an error page.
  kGuestLoadFailed = 8,

  // The guest renderer process crashed or was killed.
  kGuestProcessGone = 9,

  // The web client loaded but reported a fatal error.
  kClientError = 10,

  // Timeouts when waiting for the web client to connect or become ready.
  // In the webview world, these are owned by the chrome://glic host page.
  // In the no-webview world, GlicNoWebviewContentsManager owns the load
  // timeout.

  // The host page or manager gave up waiting for the web client to connect.
  kClientLoadTimeout = 11,

  // A warmed client never became ready after the panel was shown.
  kWarmedTimeout = 12,

  kMaxValue = kWarmedTimeout,
};
// LINT.ThenChange(//tools/metrics/structured/sync/structured.xml)

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_ENUMS_H_
