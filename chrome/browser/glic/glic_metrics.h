// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_METRICS_H_
#define CHROME_BROWSER_GLIC_GLIC_METRICS_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class Profile;

namespace glic {
class GlicEnabling;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
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
  kMaxValue = kAfterSignInDetachedAudio,
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

// LINT.IfChange(GlicRequestEvent)
// Events related to requests to the Glic API from the web client.
enum class GlicRequestEvent {
  kRequestReceived = 0,
  kRequestSent = 1,
  kRequestHandlerException = 2,
  kRequestReceivedWhileHidden = 3,
  kMaxValue = kRequestReceivedWhileHidden,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicRequestEvent)

class GlicEnabling;
class GlicFocusedTabManager;
class GlicWindowController;

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
    virtual FocusedTabData GetFocusedTabData() = 0;
  };

  GlicMetrics(Profile* profile, GlicEnabling* enabling);
  GlicMetrics(const GlicMetrics&) = delete;
  GlicMetrics& operator=(const GlicMetrics&) = delete;
  ~GlicMetrics();

  // See glic.mojom for details. These are events from the web client. The
  // lifetime of the web client is scoped to that of the window, so if these
  // methods are called then controller_ is guaranteed to exist.
  void OnUserInputSubmitted(mojom::WebClientMode mode);
  void OnResponseStarted();
  void OnResponseStopped();
  void OnSessionTerminated();
  void OnResponseRated(bool positive);
  void OnAttachedToBrowser(AttachChangeReason reason);
  void OnDetachedFromBrowser(AttachChangeReason reason);

  // ----Public API called by other glic classes-----
  // Called when the glic window starts to open.
  void OnGlicWindowOpen(bool attached, mojom::InvocationSource source);
  // Called when the glic window is open and ready.
  void OnGlicWindowOpenAndReady();
  // Called just after the the glic window has been loaded into the UI.
  void OnGlicWindowShown();
  // Called when the glic window is resized.
  void OnGlicWindowResize();
  // Called when the glic window starts being resized by the user.
  void OnWidgetUserResizeStarted();
  // Called when the glic window stops being resized by the user.
  void OnWidgetUserResizeEnded();
  // Called when the glic window finishes closing.
  void OnGlicWindowClose();
  // Called when glic requests a scroll.
  void OnGlicScrollAttempt();
  // Called when scrolling starts (after glic requests to scroll) or if
  // the operation fails. `success` is true if a scroll was successfully
  // triggered.
  void OnGlicScrollComplete(bool success);

  // Must be called immediately after constructor before any calls from
  // glic.mojom.
  void SetControllers(GlicWindowController* window_controller,
                      GlicFocusedTabManager* tab_manager);
  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate);

  // Must be called when context is requested.
  void DidRequestContextFromFocusedTab();

  void set_show_start_time(base::TimeTicks time) { show_start_time_ = time; }

  void set_starting_mode(mojom::WebClientMode mode) { starting_mode_ = mode; }

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

  // Resets the window timing state variables.
  void ResetGlicWindowPresentationTimingState();

  // These members are cleared in OnResponseStopped.
  base::TimeTicks input_submitted_time_;
  mojom::WebClientMode input_mode_;
  bool did_request_context_ = false;
  std::set<mojom::WebClientMode> inputs_modes_used_;
  int attach_change_count_ = 0;

  // Session state. `session_start_time_` is a sentinel that is cleared in
  // OnGlicWindowClose() and is used to determine whether OnGlicWindowOpen was
  // called.
  int session_responses_ = 0;
  base::TimeTicks session_start_time_;
  mojom::InvocationSource invocation_source_ =
      mojom::InvocationSource::kOsButton;

  // Used to record impressions of glic entry points.
  base::RepeatingTimer impression_timer_;

  // Used to record metrics about the glic window size.
  base::RepeatingTimer glic_window_size_timer_;

  // A context-free source id used when no web contents is targeted.
  ukm::SourceId no_url_source_id_ = ukm::NoURLSourceId();
  // The source id at the time context is requested. If context was not
  // requested then this is `no_url_source_id_`.
  ukm::SourceId source_id_ = ukm::NoURLSourceId();

  // The owner of this class is responsible for maintaining appropriate lifetime
  // for controller_.
  std::unique_ptr<Delegate> delegate_;
  raw_ptr<Profile> profile_;
  raw_ptr<GlicEnabling> enabling_;

  // Whether Glic is enabled and FRE has been completed. Tracked to trigger
  // metric(s) on change.
  bool is_enabled_ = false;

  // Set to true in OnResponseStarted() and set to false in OnResponseStopped().
  // This is a workaround and should be removed, see crbug.com/399151164.
  bool response_started_ = false;

  std::vector<base::CallbackListSubscription> subscriptions_;

  // Cache the last value of the kGlicPinnedToTabstrip pref so that we only emit
  // metrics for changes to the last value.
  bool is_pinned_ = false;
  PrefChangeRegistrar pref_registrar_;

  // The following two variables are used together for recording metrics and are
  // reset together after the metric is recorded.
  // The timestamp when the glic window starts to be shown.
  base::TimeTicks show_start_time_;
  // Web client's operation modes.
  mojom::WebClientMode starting_mode_ = mojom::WebClientMode::kUnknown;

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
  mojom::WebClientMode scroll_input_mode_;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_METRICS_H_
