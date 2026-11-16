// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_ACTOR_ATTRIBUTION_METRICS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_ACTOR_ATTRIBUTION_METRICS_H_

namespace enterprise_connectors {

// UMA histogram names for extension actor attribution. See
// `ExtensionActorTracker` and `event_initiator_utils.h`.
inline constexpr char kExtensionActorTrackerClearedHistogram[] =
    "Enterprise.ExtensionActorAttribution.Tracker.Cleared";
inline constexpr char kExtensionActorLookupDownloadResultHistogram[] =
    "Enterprise.ExtensionActorAttribution.Lookup.Download.Result";
inline constexpr char kExtensionActorLookupWebContentsResultHistogram[] =
    "Enterprise.ExtensionActorAttribution.Lookup.WebContents.Result";

// Why an active extension actor was cleared from a tab.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ExtensionActorClearReason)
enum class ExtensionActorClearReason {
  // The user interacted with the tab.
  kUserInteraction = 0,
  // A cross-document navigation not initiated by an extension committed.
  kNonExtensionNavigation = 1,
  // `ExtensionActorTracker::ClearActiveActor()` was called.
  kExplicit = 2,
  // A browser-initiated navigation not started by an extension (e.g. from the
  // omnibox or bookmarks) began.
  kBrowserInitiatedNavigation = 3,
  kMaxValue = kBrowserInitiatedNavigation,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:ExtensionActorClearReason)

// The outcome of looking up the extension that is acting on a tab.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ExtensionActorAttributionResult)
enum class ExtensionActorAttributionResult {
  // An extension was attributed, whether or not it is still installed.
  kAttributed = 0,
  // The tab has no active extension actor. This includes every tab in a
  // profile for which `ExtensionActorTracker::IsEnabled()` is false.
  kNoActor = 1,
  kMaxValue = kNoActor,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:ExtensionActorAttributionResult)

// The strongest signal identifying the extension behind a download. Signals
// are checked in the order `kDownloadedByExtension`,
// `kExtensionRequestInitiator`, `kTabActor`, and the first match is recorded.
// Buckets name the signal that was present, not whether it produced
// attribution, so they keep their meaning as more signals start being used.
// Currently only `kDownloadedByExtension` produces attribution; the other two
// size the downloads that bypass `chrome.downloads` (crbug.com/570065942).
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ExtensionDownloadAttributionSignal)
enum class ExtensionDownloadAttributionSignal {
  // None of the signals below was present.
  kNone = 0,
  // Started via the `chrome.downloads` API (`DownloadedByExtension`).
  kDownloadedByExtension = 1,
  // The request initiator is a `chrome-extension://` origin, e.g. an extension
  // popup or offscreen document navigated to the file.
  kExtensionRequestInitiator = 2,
  // The originating tab has an active extension actor.
  kTabActor = 3,
  kMaxValue = kTabActor,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:ExtensionDownloadAttributionSignal)

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_ACTOR_ATTRIBUTION_METRICS_H_
