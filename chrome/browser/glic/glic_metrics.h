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
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "components/prefs/pref_change_registrar.h"
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
  kSuccess = 0,
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
  kMaxValue = kFailedClientUnreadied,
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

// LINT.IfChange(ResponseSegmentation)
enum class ResponseSegmentation {
  kUnknown = 0,
  kOsButtonAttachedText = 1,
  kOsButtonAttachedAudio = 2,
  kOsButtonDetachedText = 3,
  kOsButtonDetachedAudio = 4,
  kOsButtonMenuAttachedText = 5,
  kOsButtonMenuAttachedAudio = 6,
  kOsButtonMenuDetachedText = 7,
  kOsButtonMenuDetachedAudio = 8,
  kOsHotkeyAttachedText = 9,
  kOsHotkeyAttachedAudio = 10,
  kOsHotkeyDetachedText = 11,
  kOsHotkeyDetachedAudio = 12,
  kButtonTopChromeAttachedText = 13,
  kButtonTopChromeAttachedAudio = 14,
  kButtonTopChromeDetachedText = 15,
  kButtonTopChromeDetachedAudio = 16,
  kFreAttachedText = 17,
  kFreAttachedAudio = 18,
  kFreDetachedText = 19,
  kFreDetachedAudio = 20,
  kProfilePickerAttachedText = 21,
  kProfilePickerAttachedAudio = 22,
  kProfilePickerDetachedText = 23,
  kProfilePickerDetachedAudio = 24,
  kNudgeAttachedText = 25,
  kNudgeAttachedAudio = 26,
  kNudgeDetachedText = 27,
  kNudgeDetachedAudio = 28,
  kThreeDotsMenuAttachedText = 29,
  kThreeDotsMenuAttachedAudio = 30,
  kThreeDotsMenuDetachedText = 31,
  kThreeDotsMenuDetachedAudio = 32,
  kUnsupportedAttachedText = 33,
  kUnsupportedAttachedAudio = 34,
  kUnsupportedDetachedText = 35,
  kUnsupportedDetachedAudio = 36,
  kWhatsNewAttachedText = 37,
  kWhatsNewAttachedAudio = 38,
  kWhatsNewDetachedText = 39,
  kWhatsNewDetachedAudio = 40,
  kAfterSignInAttachedText = 41,
  kAfterSignInAttachedAudio = 42,
  kAfterSignInDetachedText = 43,
  kAfterSignInDetachedAudio = 44,
  kSharedTabAttachedText = 45,
  kSharedTabAttachedAudio = 46,
  kSharedTabDetachedText = 47,
  kSharedTabDetachedAudio = 48,
  kActorTaskIconAttachedText = 49,
  kActorTaskIconAttachedAudio = 50,
  kActorTaskIconDetachedText = 51,
  kActorTaskIconDetachedAudio = 52,
  kHandoffButtonAttachedText = 53,
  kHandoffButtonAttachedAudio = 54,
  kHandoffButtonDetachedText = 55,
  kHandoffButtonDetachedAudio = 56,
  kMaxValue = kHandoffButtonDetachedAudio,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicResponseSegmentation)

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
  kRequestReceivedWhileHidden = 3,
  kMaxValue = kRequestReceivedWhileHidden,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicRequestEvent)

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

// LINT.IfChange(GlicTabPinnedForSharingResult)
enum class GlicTabPinnedForSharingResult {
  kPinTabForSharingFailedTooManyTabs = 0,
  kPinTabForSharingFailedNotValidForSharing = 1,
  kPinTabForSharingSucceeded = 2,
  kMaxValue = kPinTabForSharingSucceeded,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicTabPinnedForSharingResult)

// The different states of active tab sharing.
// LINT.IfChange(ActiveTabSharingState)
enum class ActiveTabSharingState {
  kActiveTabIsShared = 0,
  kCannotShareActiveTab = 1,
  kNoTabCanBeShared = 2,
  kTabContextPermissionNotGranted = 3,
  kMaxValue = kTabContextPermissionNotGranted
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:ActiveTabSharingState)

class GlicEnabling;
class GlicSharingManager;
class GlicWindowControllerInterface;

namespace internal {
class BrowserActivityObserver;
}

// Responsible for all glic web-client metrics, and all stateful glic metrics.
// Some stateless glic metrics are logged inline in the relevant code for
// convenience.
class GlicMetrics {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual gfx::Size GetWindowSize() const = 0;
    virtual bool IsWindowShowing() const = 0;
    virtual bool IsWindowAttached() const = 0;
    virtual content::WebContents* GetFocusedWebContents() = 0;
    virtual ActiveTabSharingState GetActiveTabSharingState() = 0;
    virtual int32_t GetNumPinnedTabs() const = 0;
    virtual std::vector<content::WebContents*>
    GetPinnedAndSharedWebContents() = 0;
  };

  GlicMetrics(Profile* profile, GlicEnabling* enabling);
  GlicMetrics(const GlicMetrics&) = delete;
  GlicMetrics& operator=(const GlicMetrics&) = delete;
  ~GlicMetrics();

  // See glic.mojom for details. These are events from the web client. The
  // lifetime of the web client is scoped to that of the window, so if these
  // methods are called then controller_ is guaranteed to exist.
  void OnUserInputSubmitted(mojom::WebClientMode mode);
  void OnContextUploadStarted();
  void OnContextUploadCompleted();
  void OnReaction(mojom::MetricUserInputReactionType reaction_type);
  void OnResponseStarted();
  void OnResponseStopped(mojom::ResponseStopCause cause);
  void OnSessionTerminated();
  void OnResponseRated(bool positive);
  void OnTurnCompleted(mojom::WebClientModel model, base::TimeDelta duration);
  void OnModelChanged(mojom::WebClientModel model);
  void OnRecordUseCounter(uint16_t counter);

  void OnAttachedToBrowser(AttachChangeReason reason);
  void OnDetachedFromBrowser(AttachChangeReason reason);

  // ----Public API called by other glic classes-----
  // Called when the user clicks Accept in the FRE.
  void OnFreAccepted();
  // Called when the glic window starts to open.
  void OnGlicWindowStartedOpening(bool attached,
                                  mojom::InvocationSource source);
  // Called to signal that the Glic window opening was interrupted for some
  // reason (e.g, an error happened, reached a login page instead of the web
  // client, etc).
  void OnGlicWindowOpenInterrupted();
  // Called just after the the glic window has been loaded into the UI.
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
  // Called when the glic window finishes closing.
  void OnGlicWindowClose(Browser* last_active_browser,
                         std::optional<display::Display> display,
                         const gfx::Rect& glic_bounds);
  // Called when glic requests a scroll.
  void OnGlicScrollAttempt();
  // Called when scrolling starts (after glic requests to scroll) or if
  // the operation fails. `success` is true if a scroll was successfully
  // triggered.
  void OnGlicScrollComplete(bool success);

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

  // One of these three must be called immediately after constructor before any
  // calls from glic.mojom.
  void SetControllers(GlicWindowControllerInterface* window_controller,
                      GlicSharingManager* sharing_manager);
  void SetControllersWithInstance(GlicInstance* glic_instance,
                                  GlicSharingManager* sharing_manager);
  void ClearControllers();

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate);

  // Must be called when context is requested from a tab.
  void DidRequestContextFromTab(content::WebContents& web_contents);

  // Sets the input mode of the web client. Should be called when the panel is
  // opened and in every subsequent mode change.
  void SetWebClientMode(mojom::WebClientMode mode);

  mojom::WebClientModel current_model() const { return current_model_; }

 private:
  // Called when `impression_timer_` fires.
  void OnImpressionTimerFired();

  // Called when `glic_window_size_timer_` fires.
  void OnGlicWindowSizeTimerFired();

  // Stores the source id at the time that context is requested.
  void StoreSourceId();

  // Called when kGlicCompletedFre or GlicEnabling::IsAllowed() changes.
  void OnMaybeEnabledAndConsentForProfileChanged();

  // Called when kGlicPinnedToTabstrip changes.
  void OnPinningPrefChanged();

  // Called when kGlicTabContextEnabled changes.
  void OnTabContextEnabledPrefChanged();

  // Returns the area in the display a given center point is.
  DisplayPosition GetDisplayPositionOfPoint(
      std::optional<display::Display> display,
      const gfx::Point& glic_center_point);

  // Returns the area relative to the given chrome browser a given center point
  // is.
  ChromeRelativePosition GetChromeRelativePositionOfPoint(
      Browser* browser,
      const gfx::Point& glic_center_point);

  // Returns the percent overlap of the given glic bounds and the given chrome
  // browser.
  PercentOverlap GetPercentOverlapWithBrowser(Browser* browser,
                                              const gfx::Rect& glic_bounds);

  base::TimeTicks fre_accepted_time_;

  // These members are cleared in OnResponseStopped.
  struct TurnInfo {
    base::TimeTicks input_submitted_time_;
    // Set to true in OnResponseStarted() and set to false in
    // OnResponseStopped(). This is a workaround and should be removed, see
    // crbug.com/399151164.
    bool response_started_ = false;
    bool reported_reaction_time_canned_ = false;
    bool reported_reaction_time_modelled_ = false;
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

  mojom::WebClientModel current_model_ = mojom::WebClientModel::kDefault;

  // Session state. `session_start_time_` is a sentinel that is cleared in
  // OnGlicWindowClose() and is used to determine whether
  // OnGlicWindowStartedOpening was called.
  int session_responses_ = 0;
  base::TimeTicks session_start_time_;
  mojom::InvocationSource invocation_source_ =
      mojom::InvocationSource::kOsButton;

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

  std::vector<base::CallbackListSubscription> subscriptions_;

  // Cache the last value of the kGlicPinnedToTabstrip pref so that we only emit
  // metrics for changes to the last value.
  bool is_pinned_ = false;
  PrefChangeRegistrar pref_registrar_;

  // The following two variables are used together for recording metrics and are
  // reset together after the metric is recorded.
  // The timestamp when the glic window starts to be shown.
  base::TimeTicks show_start_time_;

  // The following variables are used for recording scroll related metrics.
  // The number of scroll attempts  (tracked per session and reset when the
  // session ends).
  int scroll_attempt_count_ = 0;
  // These two variables mirror `input_submitted_time_` and
  // `input_mode_`, but are only set when `OnGlicScrollAttempt()` is
  // called. They are reset in `OnGlicScrollComplete()`. They are separately
  // tracked because `OnGlicScrollComplete()` could potentially be called after
  // `OnResponseStopped()`, which resets `input_submitted_time_` and
  // `input_mode_`.
  base::TimeTicks scroll_input_submitted_time_;
  mojom::WebClientMode scroll_input_mode_ = mojom::WebClientMode::kUnknown;

  std::optional<base::TimeTicks> last_upload_start_time_;

  // The time the last attempt to share an image started.
  base::TimeTicks share_image_start_time_;

  std::unique_ptr<internal::BrowserActivityObserver> browser_activity_observer_;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_METRICS_H_
