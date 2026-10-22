// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_METRICS_H_
#define CHROME_BROWSER_GLIC_GLIC_METRICS_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace glic {
class GlicEnabling;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// LINT.IfChange(ShareImageResult)
enum class ShareImageResult {
  kSentImageToClient = 0,
  kFailedNoTab = 1,
  kFailedNoFrame = 2,
  kFailedNoBrowser = 3,
  kFailedTimedOut = 4,
  kFailedNoImage = 5,
  kFailedReplacedByNewShare = 6,
  kFailedNoTabContext = 7,
  kFailedSawNavigation = 8,
  kFailedDiscardedContents = 9,
  kFailedDetachedTab = 10,
  kFailedClipboardCopyPolicy = 11,
  kFailedClipboardPastePolicy = 12,
  kFailedNoInstance = 13,
  kFailedClientUnreadied = 14,
  kFailedTimedOutNoInstance = 15,
  kFailedTimedOutNoWebClient = 16,
  kFailedTimedOutDidNotCompleteOnboarding = 17,
  kFailedLostInstance = 18,
  kFailedSawNavigationDidNotCompleteOnboarding = 19,
  kFailedUnknown = 20,
  kFailedInvalidConversationId = 21,
  kFailedInvokeInProgress = 22,
  kFailedInvalidConfiguration = 23,
  kFailedNoClientFrame = 24,
  kFailedNoClipboardMetadata = 25,
  kFailedCancelled = 26,
  kFailedProfileNotEnabled = 27,
  kFailedSuperseded = 28,
  kFailedLiveModeActive = 29,
  kMaxValue = kFailedLiveModeActive,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:ShareImageResult)

// LINT.IfChange(EntryPointStatus)
enum class EntryPointStatus {
  kBeforeFreNotEligible = 0,
  kBeforeFreAndEligible = 1,
  kIncompleteFreNotEligible = 2,
  kIncompleteFreAndEligible = 3,
  kAfterFreBrowserOnly = 4,
  kAfterFreOsOnly = 5,
  kAfterFreBrowserAndOs = 6,
  kAfterFreThreeDotOnly = 7,
  kAfterFreNotEligible = 8,
  kAfterFreAnchoredButIneligible = 9,
  kMaxValue = kAfterFreAnchoredButIneligible,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicEntryPointStatus)

// LINT.IfChange(GlicInputModesUsed)
enum class InputModesUsed {
  kNone = 0,
  kOnlyText = 1,
  kOnlyAudio = 2,
  kTextAndAudio = 3,

  kMaxValue = kTextAndAudio,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicInputModesUsed)

// Events related to requests to the Glic API from the web client.
// LINT.IfChange(GlicRequestEvent)
enum class GlicRequestEvent {
  kRequestReceived = 0,
  kRequestSent = 1,
  kRequestHandlerException = 2,
  // Deprecated: kRequestReceivedWhileHidden = 3,
  kRequestReceivedWhileInactive = 4,
  kMaxValue = kRequestReceivedWhileInactive,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicRequestEvent)

// LINT.IfChange(GlicTabPinnedForSharingResult)
enum class GlicTabPinnedForSharingResult {
  kPinTabForSharingFailedTooManyTabs = 0,
  kPinTabForSharingFailedNotValidForSharing = 1,
  kPinTabForSharingSucceeded = 2,
  kMaxValue = kPinTabForSharingSucceeded,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicTabPinnedForSharingResult)

// LINT.IfChange(GlicOptInFlowSource)
enum class OptInFlow {
  kGlicFre = 0,
  kExperimentalTriggering = 1,
  kMaxValue = kExperimentalTriggering,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicOptInFlowSource)

// Records profile-scoped glic metrics, such as FRE/onboarding, entry point
// and preference metrics. Per-instance metrics are recorded by
// `GlicInstanceMetrics`. Some stateless glic metrics are logged inline in the
// relevant code for convenience.
class GlicMetrics {
 public:
  GlicMetrics(Profile* profile, GlicEnabling* enabling);
  GlicMetrics(const GlicMetrics&) = delete;
  GlicMetrics& operator=(const GlicMetrics&) = delete;
  ~GlicMetrics();

  // Called when the web client reports submitted user input. Records the
  // FRE-to-first-query metrics once if the FRE was accepted earlier in this
  // browser session.
  void OnUserInputSubmitted();

  // See glic.mojom for details. These are events from the web client.
  void OnContextUploadStarted();
  void OnContextUploadCompleted();
  void OnSessionTerminated();
  void OnResponseRated(bool positive);

  // ----Public API called by other glic classes-----
  // Called when the web client reports that the user completed the
  // FRE (consented).
  void OnTrustFirstOnboardingAccept();
  // Called whenever an embedder (side panel or floating window) of any glic
  // instance is closed. Idempotent: if the web client FRE was shown and not
  // accepted, logs dismiss metrics and clears `onboarding_shown_time_`;
  // otherwise a no-op.
  void OnInstanceClosed();
  // Called when the user accepts the web client FRE (from
  // OnTrustFirstOnboardingAccept()). Records the acceptance time and
  // invocation source used by the FRE-to-first-query metrics logged in
  // OnUserInputSubmitted().
  void OnFreAccepted();
  // Called when an opt-in dialog or onboarding flow is shown.
  void OnOptInShown(OptInFlow flow);
  // Called when an opt-in dialog or onboarding flow finishes loading content.
  void OnOptInImpression(OptInFlow flow);
  // Called when an opt-in dialog or onboarding flow is accepted.
  void OnOptInAccepted(OptInFlow flow);
  // Called when an opt-in dialog or onboarding flow is implicitly dismissed.
  void OnOptInDismissed(OptInFlow flow);
  // Called when an opt-in dialog or onboarding flow is explicitly rejected.
  void OnOptInRejected(OptInFlow flow);
  // Called by GlicInstanceImpl::Show() on every initial show of an embedder
  // (side panel or floating window) of any glic instance: the first time it
  // is shown, and again each time it is shown after being closed. Overwrites
  // `invocation_source_`, may record web client FRE "shown" metrics, and
  // records open metrics (see the implementation).
  void OnGlicWindowStartedOpening(bool attached,
                                  mojom::InvocationSource source);

  // Called when a tab is pinned for sharing with glic. `result` indicates
  // whether the pinning succeeded or why it failed.
  void OnTabPinnedForSharing(GlicTabPinnedForSharingResult result);

  // Called when a response is received with closed captions showing.
  void LogClosedCaptionsShown();

  // Called when an attempt to share an image with glic is begun.
  void OnShareImageStarted();

  // Called when an attempt to share an image with glic completes.
  void OnShareImageComplete(ShareImageResult result);

  // Called when a tab is activated via the glic page.
  void OnActivateTabFromInstance(tabs::TabInterface* tab);

  // Logs an error that occurred while trying to get context from the focused
  // tab.
  void LogGetContextFromFocusedTabError(GlicGetContextFromTabError error);

  // Logs an error that occurred while trying to get context from an arbitrary
  // tab.
  void LogGetContextFromTabError(GlicGetContextFromTabError error);

  // Logs an error that occurred while an actor tried to get context from an
  // arbitrary tab.
  void LogGetContextForActorFromTabError(GlicGetContextFromTabError error);

  // Logs an error that occurred while trying to get image bytes from an
  // arbitrary tab.
  void LogGetImageBytesFromTabError(GlicGetContextFromTabError error);

  // Records user preferences for the profile. Called when the GlicKeyedService
  // for each profile is created.
  void RecordGlicProfilePreferences();

  // Sets the input mode of the web client. Called when the web client reports
  // its mode as the panel opens, and on every subsequent mode change.
  void SetWebClientMode(mojom::WebClientMode mode);

 private:
  // Called from OnGlicWindowStartedOpening(). If consent has not been given
  // and the web client FRE is not already showing, records FRE "shown"
  // metrics and sets `onboarding_shown_time_`. Otherwise this is a no-op.
  void OnInstanceOpened();

  // Called when `impression_timer_` fires.
  void OnImpressionTimerFired();

  // Called when consent or GlicEnabling::IsAllowed() changes.
  void OnMaybeEnabledAndConsentForProfileChanged();

  // Called when kGlicPinnedToTabstrip changes.
  void OnPinningPrefChanged();

  // Records the time from startup until Glic was enabled for the profile.
  void RecordStartupEnablement();

  // Time at which the FRE was accepted (set in OnFreAccepted()). Cleared
  // after the FRE-to-first-query metrics are recorded in
  // OnUserInputSubmitted(); null otherwise.
  base::TimeTicks fre_accepted_time_;

  // The current web client input mode, as last reported via
  // SetWebClientMode(). Used to suffix the Glic.Api.GetContext*.Error
  // histograms.
  mojom::WebClientMode input_mode_ = mojom::WebClientMode::kUnknown;

  // Invocation source passed to the most recent OnGlicWindowStartedOpening()
  // call, so it is stored on every initial show of an embedder of any
  // instance (see there). Recorded to
  // Glic.Fre.{Shown,Accept,Dismissed}.InvocationSource and snapshotted into
  // `onboarding_invocation_source_` by OnFreAccepted().
  mojom::InvocationSource invocation_source_ =
      mojom::InvocationSource::kUnsupported;

  // Snapshot of `invocation_source_` taken in OnFreAccepted(), i.e. the
  // source of the most recent glic open before the user accepted the FRE.
  // Kept separately because `invocation_source_` is overwritten whenever
  // glic is opened again before the first query. Recorded to
  // Glic.Fre.UserInput.InvocationSource by the first OnUserInputSubmitted()
  // call after FRE acceptance (while `fre_accepted_time_` is set). Not reset
  // after use.
  mojom::InvocationSource onboarding_invocation_source_ =
      mojom::InvocationSource::kUnsupported;

  // Used to record impressions of glic entry points.
  base::RepeatingTimer impression_timer_;

  raw_ptr<Profile> profile_;
  raw_ptr<GlicEnabling> enabling_;

  // Whether Glic is enabled and FRE has been completed. Tracked to trigger
  // metric(s) on change.
  bool is_enabled_ = false;

  // Whether we have already recorded the metric that tracks how long it took
  // for Glic to be enabled since startup.
  bool recorded_startup_enablement_ = false;

  std::vector<base::CallbackListSubscription> subscriptions_;

  // Cache the last value of the kGlicPinnedToTabstrip pref so that we only emit
  // metrics for changes to the last value.
  bool is_pinned_ = false;
  PrefChangeRegistrar pref_registrar_;

  // The time the web client FRE was shown, or null if it is not
  // currently showing. Set in OnInstanceOpened() and reset, after recording
  // Glic.Fre.TotalTime.*, when the onboarding is accepted or dismissed.
  base::TimeTicks onboarding_shown_time_;

  // Start time of the in-flight context upload, if any.
  std::optional<base::TimeTicks> last_upload_start_time_;

  // The time the last attempt to share an image started.
  base::TimeTicks share_image_start_time_;
};

// Records the hotkey cleared preference metric.
void RecordHotkeyCleared(prefs::DefaultHotkeyScope default_scope,
                         bool is_global,
                         bool launcher_enabled);

// Records the hotkey scope change preference metric.
void RecordHotkeyScopeChange(GlicHotkeyScopeChange scope_change);

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_METRICS_H_
