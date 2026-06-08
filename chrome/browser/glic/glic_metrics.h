// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_METRICS_H_
#define CHROME_BROWSER_GLIC_GLIC_METRICS_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_instance_metrics_backwards_compatibility.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/display/display.h"

class Profile;
class Browser;

namespace glic {
class GlicEnabling;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// LINT.IfChange(ChromeRelativePosition)
enum class ChromeRelativePosition {
  kAboveLeft = 0,
  kCenterLeft = 1,
  kBelowLeft = 2,
  kAboveCenter = 3,
  kOverlap = 4,
  kBelowCenter = 5,
  kAboveRight = 6,
  kCenterRight = 7,
  kBelowRight = 8,
  kChromeOnOtherDisplay = 9,
  kNoVisibleChromeBrowser = 10,
  kMaxValue = kNoVisibleChromeBrowser,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:ChromeRelativePosition)

// LINT.IfChange(DisplayPosition)
enum class DisplayPosition {
  kTopLeft = 0,
  kCenterLeft = 1,
  kBottomLeft = 2,
  kTopCenter = 3,
  kCenterCenter = 4,
  kBottomCenter = 5,
  kTopRight = 6,
  kCenterRight = 7,
  kBottomRight = 8,
  kUnknown = 9,
  kMaxValue = kUnknown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:DisplayPosition)

// LINT.IfChange(PercentOverlap)
enum class PercentOverlap {
  k0 = 0,
  k10 = 1,
  k20 = 2,
  k30 = 3,
  k40 = 4,
  k50 = 5,
  k60 = 6,
  k70 = 7,
  k80 = 8,
  k90 = 9,
  k100 = 10,
  kNoVisibleChromeBrowser = 11,
  kMaxValue = kNoVisibleChromeBrowser,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:PercentOverlap)

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
  kMaxValue = kFailedSawNavigationDidNotCompleteOnboarding,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:ShareImageResult)

// LINT.IfChange(Error)
enum class Error {
  kResponseStartWithoutInput = 0,
  kResponseStopWithoutInput = 1,
  kResponseStartWhileHidingOrHidden = 2,
  kWindowCloseWithoutWindowOpen = 3,
  kMaxValue = kWindowCloseWithoutWindowOpen,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicResponseError)

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
  kMaxValue = kAfterFreNotEligible,
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

// LINT.IfChange(AttachChangeReason)
enum class AttachChangeReason {
  // Attach state changed because of a drag gesture.
  kDrag = 0,
  // Attach state changed because of a menu click.
  kMenu = 1,
  // Attachment state initialized.
  kInit = 2,

  kMaxValue = kInit,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicAttachChangeReason)

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

class GlicEnabling;
class GlicSharingManager;

namespace internal {
class BrowserActivityObserver;
}

// Responsible for all glic web-client metrics, and all stateful glic metrics.
// Some stateless glic metrics are logged inline in the relevant code for
// convenience.
class GlicMetrics : public GlicInstanceMetricsBackwardsCompatibility {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual gfx::Size GetWindowSize() const = 0;
    virtual bool IsWindowShowing() const = 0;
    virtual bool IsWindowAttached() const = 0;
    virtual content::WebContents* GetFocusedWebContents() = 0;
    virtual int32_t GetNumPinnedTabs() const = 0;
    virtual std::vector<content::WebContents*>
    GetPinnedAndSharedWebContents() = 0;
  };

  GlicMetrics(Profile* profile, GlicEnabling* enabling);
  GlicMetrics(const GlicMetrics&) = delete;
  GlicMetrics& operator=(const GlicMetrics&) = delete;
  ~GlicMetrics() override;

  // `GlicInstanceMetricsBackwardsCompatibility`:
  void OnUserInputSubmitted(mojom::WebClientMode mode) override;
  void OnResponseStarted() override;
  void OnResponseStopped(mojom::ResponseStopCause cause) override;
  void DidRequestContextFromTab(tabs::TabInterface& tab) override;

  // See glic.mojom for details. These are events from the web client. The
  // lifetime of the web client is scoped to that of the window, so if these
  // methods are called then controller_ is guaranteed to exist.
  void OnContextUploadStarted();
  void OnContextUploadCompleted();
  void OnSessionTerminated();
  void OnResponseRated(bool positive);

  void OnAttachedToBrowser(AttachChangeReason reason);
  void OnDetachedFromBrowser(AttachChangeReason reason);

  // ----Public API called by other glic classes-----
  // Called when the user completes the onboarding flow (consents).
  void OnTrustFirstOnboardingAccept();
  // Called when any instance is closed. This method is idempotent. If
  // trust-first FRE was shown and not accepted, this metric logs a dismiss
  // metric, and then clears the bit tracking FRE open.
  void OnInstanceClosed();
  // Called when the user clicks Accept in the FRE.
  void OnFreAccepted();
  // Called when the glic window starts to open.
  void OnGlicWindowStartedOpening(bool attached,
                                  mojom::InvocationSource source);
  // Called to signal that the Glic window opening was interrupted for some
  // reason (e.g, an error happened, reached a login page instead of the web
  // client, etc).
  void OnGlicWindowOpenInterrupted();
  // Called just after the glic window has been loaded into the UI.
  void OnGlicWindowShown(Browser* browser,
                         std::optional<display::Display> glic_display,
                         const gfx::Rect& glic_bounds);
  // Called when the glic window has been opened and is ready.
  void OnGlicWindowOpenAndReady();
  // Called when the glic window is resized.
  void OnGlicWindowResize();
  // Called when the glic window starts being resized by the user.
  void OnWidgetUserResizeStarted();
  // Called when the glic window stops being resized by the user.
  void OnWidgetUserResizeEnded();
  // Called when the detached glic window finishes closing.
  void OnGlicWindowClose(Browser* last_active_browser,
                         std::optional<display::Display> display,
                         const gfx::Rect& glic_bounds);

  // Called when a tab is pinned for sharing with glic. `success` is true if the
  // pinning was successful.
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

  // One of these must be called immediately after constructor before any
  // calls from glic.mojom.
  void SetControllersWithInstance(GlicInstance* glic_instance,
                                  GlicSharingManager* sharing_manager);
  void ClearControllers();

  // Records user preferences for the profile. Called when the GlicKeyedService
  // for each profile is created.
  void RecordGlicProfilePreferences();

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate);

  // Sets the input mode of the web client. Should be called when the panel is
  // opened and in every subsequent mode change.
  void SetWebClientMode(mojom::WebClientMode mode);

 private:
  // Called when any instance is opened. This method is used to track whether an
  // FRE onboarding is going to be shown. If an FRE onboarding is already shown,
  // this method is idempotent.
  void OnInstanceOpened();

  // Called when `impression_timer_` fires.
  void OnImpressionTimerFired();

  // Called when `glic_window_size_timer_` fires.
  void OnGlicWindowSizeTimerFired();

  // Stores the source id at the time that context is requested.
  void StoreSourceId();

  // Called when kGlicCompletedFre or GlicEnabling::IsAllowed() changes.
  void OnMaybeEnabledAndConsentForProfileChanged();

  // Records the time from startup until Glic was enabled for the profile.
  void RecordStartupEnablement();

  // Returns the area in the display a given center point is.
  DisplayPosition GetDisplayPositionOfPoint(
      std::optional<display::Display> display,
      const gfx::Point& glic_center_point);

#if !BUILDFLAG(IS_ANDROID)
  // Returns the area relative to the given chrome browser a given center point
  // is.
  ChromeRelativePosition GetChromeRelativePositionOfPoint(
      Browser* browser,
      const gfx::Point& glic_center_point);
  // Returns the percent overlap of the given glic bounds and the given chrome
  // browser.
  PercentOverlap GetPercentOverlapWithBrowser(Browser* browser,
                                              const gfx::Rect& glic_bounds);
#endif

  base::TimeTicks fre_accepted_time_;

  // These members are cleared in OnResponseStopped.
  struct TurnInfo {
    base::TimeTicks input_submitted_time_;
    // Set to true in OnResponseStarted() and set to false in
    // OnResponseStopped(). This is a workaround and should be removed, see
    // crbug.com/399151164.
    bool response_started_ = false;
    // A chosen source id from which context was requested.
    ukm::SourceId chosen_source_id_ = ukm::NoURLSourceId();
  };

  // Tracks information related to individual request/response turns.
  // It is reset when new user input is submitted and populated as the turn
  // progresses. It is also reset at when the response stops.
  TurnInfo turn_;

  // The last web client input mode used by the user.
  mojom::WebClientMode input_mode_ = mojom::WebClientMode::kUnknown;
  std::set<mojom::WebClientMode> inputs_modes_used_;
  int attach_change_count_ = 0;

  // Tracks the source ID from the latest tab context requested by the web
  // client. It is reset when user input is submitted.
  ukm::SourceId last_tab_context_source_id_ = ukm::NoURLSourceId();

  // Session state. `session_start_time_` is a sentinel that is cleared in
  // OnGlicWindowClose() and is used to determine whether
  // OnGlicWindowStartedOpening was called.
  int session_responses_ = 0;
  base::TimeTicks session_start_time_;
  mojom::InvocationSource invocation_source_ =
      mojom::InvocationSource::kUnsupported;
  mojom::InvocationSource onboarding_invocation_source_ =
      mojom::InvocationSource::kUnsupported;

  // Used to record impressions of glic entry points.
  base::RepeatingTimer impression_timer_;

  // Used to record metrics about the glic window size.
  base::RepeatingTimer glic_window_size_timer_;

  // The owner of this class is responsible for maintaining appropriate lifetime
  // for controller_.
  std::unique_ptr<Delegate> delegate_;
  raw_ptr<Profile> profile_;
  raw_ptr<GlicEnabling> enabling_;

  // Whether Glic is enabled and FRE has been completed. Tracked to trigger
  // metric(s) on change.
  bool is_enabled_ = false;

  // Whether we have already recorded the metric that tracks how long it took
  // for Glic to be enabled since startup.
  bool recorded_startup_enablement_ = false;

  std::vector<base::CallbackListSubscription> subscriptions_;

  // The following two variables are used together for recording metrics and are
  // reset together after the metric is recorded.
  // The timestamp when the glic window starts to be shown.
  base::TimeTicks show_start_time_;

  // The timestamp when the onboarding flow was shown.
  base::TimeTicks onboarding_shown_time_;

  // The following variables are used for recording scroll related metrics.
  // The number of scroll attempts  (tracked per session and reset when the
  // session ends).
  int scroll_attempt_count_ = 0;

  std::optional<base::TimeTicks> last_upload_start_time_;

  // The time the last attempt to share an image started.
  base::TimeTicks share_image_start_time_;

  std::unique_ptr<internal::BrowserActivityObserver> browser_activity_observer_;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_METRICS_H_
