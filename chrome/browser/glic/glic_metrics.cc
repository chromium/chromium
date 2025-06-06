// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/accelerators/accelerator.h"

namespace glic {

namespace {

bool CheckFreStatus(Profile* profile, prefs::FreStatus status) {
  return profile->GetPrefs()->GetInteger(prefs::kGlicCompletedFre) ==
         static_cast<int>(status);
}

class DelegateImpl : public GlicMetrics::Delegate {
 public:
  explicit DelegateImpl(GlicWindowController* window_controller,
                        GlicFocusedTabManager* focus_tab_manager)
      : window_controller_(window_controller),
        focus_tab_manager_(focus_tab_manager) {}
  gfx::Size GetWindowSize() const override {
    return window_controller_->GetSize();
  }
  bool IsWindowShowing() const override {
    return window_controller_->IsShowing();
  }
  bool IsWindowAttached() const override {
    return window_controller_->IsAttached();
  }
  FocusedTabData GetFocusedTabData() override {
    return focus_tab_manager_->GetFocusedTabData();
  }

 private:
  raw_ptr<GlicWindowController> window_controller_;
  raw_ptr<GlicFocusedTabManager> focus_tab_manager_;
};

constexpr char kHistogramGlicPanelPresentationTime[] =
    "Glic.PanelPresentationTime2";

constexpr static base::TimeDelta kLogSizeMetricsDelay = base::Minutes(3);

enum class ModeOffset : int {
  kTextAttached = 1,
  kAudioAttached = 2,
  kTextDetached = 3,
  kAudioDetached = 4,
  kMaxValue = kAudioDetached,
};

ResponseSegmentation GetResponseSegmentation(bool attached,
                                             mojom::WebClientMode mode,
                                             mojom::InvocationSource source) {
  if (mode == mojom::WebClientMode::kUnknown) {
    return ResponseSegmentation::kUnknown;
  }

  ModeOffset modeOffset;
  if (mode == mojom::WebClientMode::kText && attached) {
    modeOffset = ModeOffset::kTextAttached;
  } else if (mode == mojom::WebClientMode::kAudio && attached) {
    modeOffset = ModeOffset::kAudioAttached;
  } else if (mode == mojom::WebClientMode::kText && !attached) {
    modeOffset = ModeOffset::kTextDetached;
  } else {
    modeOffset = ModeOffset::kAudioDetached;
  }

  int baseIndex =
      static_cast<int>(source) * (static_cast<int>(ModeOffset::kMaxValue));
  int offset = static_cast<int>(modeOffset);

  return static_cast<ResponseSegmentation>(baseIndex + offset);
}
}  // namespace

GlicMetrics::GlicMetrics(Profile* profile, GlicEnabling* enabling)
    : profile_(profile), enabling_(enabling) {
  impression_timer_.Start(
      FROM_HERE, base::Minutes(15),
      base::BindRepeating(&GlicMetrics::OnImpressionTimerFired,
                          base::Unretained(this)));

  subscriptions_.push_back(
      enabling_->RegisterAllowedChanged(base::BindRepeating(
          &GlicMetrics::OnMaybeEnabledAndConsentForProfileChanged,
          base::Unretained(this))));

  is_enabled_ = enabling_->IsEnabledAndConsentForProfile(profile_);
  is_pinned_ = profile_->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kGlicCompletedFre,
      base::BindRepeating(
          &GlicMetrics::OnMaybeEnabledAndConsentForProfileChanged,
          base::Unretained(this)));
  pref_registrar_.Add(prefs::kGlicPinnedToTabstrip,
                      base::BindRepeating(&GlicMetrics::OnPinningPrefChanged,
                                          base::Unretained(this)));
}
GlicMetrics::~GlicMetrics() = default;

void GlicMetrics::OnUserInputSubmitted(mojom::WebClientMode mode) {
  base::RecordAction(base::UserMetricsAction("GlicResponseInputSubmit"));
  input_submitted_time_ = base::TimeTicks::Now();
  input_mode_ = mode;
  inputs_modes_used_.insert(mode);
}

void GlicMetrics::OnResponseStarted() {
  response_started_ = true;
  base::RecordAction(base::UserMetricsAction("GlicResponseStart"));

  // It doesn't make sense to record response start without input submission.
  if (input_submitted_time_.is_null()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kResponseStartWithoutInput);
    return;
  }

  if (!delegate_->IsWindowShowing()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kResponseStartWhileHidingOrHidden);
    return;
  }

  base::TimeDelta start_time = base::TimeTicks::Now() - input_submitted_time_;
  base::UmaHistogramMediumTimes("Glic.Response.StartTime", start_time);
  switch (input_mode_) {
    case mojom::WebClientMode::kUnknown:
      base::UmaHistogramMediumTimes("Glic.Response.StartTime.InputMode.Unknown",
                                    start_time);
      break;
    case mojom::WebClientMode::kText:
      base::UmaHistogramMediumTimes("Glic.Response.StartTime.InputMode.Text",
                                    start_time);
      break;
    case mojom::WebClientMode::kAudio:
      base::UmaHistogramMediumTimes("Glic.Response.StartTime.InputMode.Audio",
                                    start_time);
      break;
  }

  if (did_request_context_) {
    base::UmaHistogramMediumTimes("Glic.Response.StartTime.WithContext",
                                  start_time);
  } else {
    base::UmaHistogramMediumTimes("Glic.Response.StartTime.WithoutContext",
                                  start_time);
  }
  base::RecordAction(base::UserMetricsAction("GlicResponse"));
  ++session_responses_;

  // More detailed metrics.
  bool attached = delegate_->IsWindowAttached();
  base::UmaHistogramBoolean("Glic.Response.Attached", attached);
  base::UmaHistogramEnumeration("Glic.Response.InvocationSource",
                                invocation_source_);
  base::UmaHistogramEnumeration("Glic.Response.InputMode", input_mode_);
  base::UmaHistogramEnumeration(
      "Glic.Response.Segmentation",
      GetResponseSegmentation(attached, input_mode_, invocation_source_));

  ukm::builders::Glic_Response(source_id_)
      .SetAttached(attached)
      .SetInvocationSource(static_cast<int64_t>(invocation_source_))
      .SetWebClientMode(static_cast<int64_t>(input_mode_))
      .Record(ukm::UkmRecorder::Get());
}

void GlicMetrics::OnResponseStopped() {
  // The client may call "stopped" without "started" for very short responses.
  // We synthetically call it ourselves in this case.
  if (!input_submitted_time_.is_null() && !response_started_) {
    OnResponseStarted();
  }

  base::RecordAction(base::UserMetricsAction("GlicResponseStop"));

  if (input_submitted_time_.is_null()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kResponseStopWithoutInput);
  } else {
    base::TimeTicks now = base::TimeTicks::Now();
    base::UmaHistogramMediumTimes("Glic.Response.StopTime",
                                  now - input_submitted_time_);
  }

  // Reset all times.
  input_submitted_time_ = base::TimeTicks();
  did_request_context_ = false;
  source_id_ = no_url_source_id_;
  response_started_ = false;
}

void GlicMetrics::OnSessionTerminated() {
  base::RecordAction(base::UserMetricsAction("GlicWebClientSessionEnd"));
}

void GlicMetrics::OnResponseRated(bool positive) {
  base::UmaHistogramBoolean("Glic.Response.Rated", positive);
}

void GlicMetrics::OnGlicWindowOpen(bool attached,
                                   mojom::InvocationSource source) {
  base::RecordAction(base::UserMetricsAction("GlicSessionBegin"));
  session_start_time_ = base::TimeTicks::Now();
  invocation_source_ = source;
  base::UmaHistogramBoolean("Glic.Session.Open.Attached", attached);
  base::UmaHistogramEnumeration("Glic.Session.Open.InvocationSource", source);

  ukm::builders::Glic_WindowOpen(source_id_)
      .SetAttached(attached)
      .SetInvocationSource(static_cast<int64_t>(source))
      .Record(ukm::UkmRecorder::Get());

  const base::Time last_dismissed_time =
      profile_->GetPrefs()->GetTime(prefs::kGlicWindowLastDismissedTime);
  if (!last_dismissed_time.is_null()) {
    base::TimeDelta elapsed_time_from_last_session =
        base::Time::Now() - last_dismissed_time;
    base::UmaHistogramCounts10M(
        "Glic.PanelWebUi.ElapsedTimeBetweenSessions",
        base::saturated_cast<int>(elapsed_time_from_last_session.InSeconds()));
  }

  // Update the last dismissed timestamp. The pref might not get updated on
  // ungraceful shutdowns. As such, by updating the pref on opening the Glic
  // window, the dismissal timestamp will get approximated by the opening
  // timestamp, instead of the previously dismissal timestamp.
  profile_->GetPrefs()->SetTime(prefs::kGlicWindowLastDismissedTime,
                                base::Time::Now());
}

void GlicMetrics::OnGlicWindowOpenAndReady() {
  if (show_start_time_.is_null()) {
    return;
  }

  // Record the presentation time of showing the glic panel in an UMA histogram.
  std::string input_mode;
  if (starting_mode_ == mojom::WebClientMode::kText) {
    input_mode = ".Text";
  } else if (starting_mode_ == mojom::WebClientMode::kAudio) {
    input_mode = ".Audio";
  }
  base::TimeDelta presentation_time = base::TimeTicks::Now() - show_start_time_;
  base::UmaHistogramCustomTimes(
      base::StrCat({kHistogramGlicPanelPresentationTime, ".All"}),
      presentation_time, base::Milliseconds(1), base::Seconds(60), 50);
  if (starting_mode_ != mojom::WebClientMode::kUnknown) {
    base::UmaHistogramCustomTimes(
        base::StrCat({kHistogramGlicPanelPresentationTime, input_mode}),
        presentation_time, base::Milliseconds(1), base::Seconds(60), 50);
  }

  ResetGlicWindowPresentationTimingState();
}

void GlicMetrics::OnGlicWindowShown() {
  GlicMetrics::OnGlicWindowSizeTimerFired();
  glic_window_size_timer_.Start(
      FROM_HERE, kLogSizeMetricsDelay,
      base::BindRepeating(&GlicMetrics::OnGlicWindowSizeTimerFired,
                          base::Unretained(this)));
}

void GlicMetrics::OnGlicWindowResize() {
  base::RecordAction(base::UserMetricsAction("GlicPanelResized"));
}

void GlicMetrics::OnWidgetUserResizeStarted() {
  base::RecordAction(base::UserMetricsAction("GlicPanelUserResizeStarted"));

  gfx::Size size_on_user_resize_started = delegate_->GetWindowSize();
  base::UmaHistogramCounts10000("Glic.PanelWebUi.UserResizeStarted.Width",
                                size_on_user_resize_started.width());
  base::UmaHistogramCounts10000("Glic.PanelWebUi.UserResizeStarted.Height",
                                size_on_user_resize_started.height());
}

void GlicMetrics::OnWidgetUserResizeEnded() {
  base::RecordAction(base::UserMetricsAction("GlicPanelUserResizeEnded"));

  gfx::Size size_on_user_resize_ended = delegate_->GetWindowSize();
  base::UmaHistogramCounts10000("Glic.PanelWebUi.UserResizeEnded.Width",
                                size_on_user_resize_ended.width());
  base::UmaHistogramCounts10000("Glic.PanelWebUi.UserResizeEnded.Height",
                                size_on_user_resize_ended.height());
}

void GlicMetrics::OnGlicWindowClose() {
  base::RecordAction(base::UserMetricsAction("GlicSessionEnd"));
  base::UmaHistogramCounts1000("Glic.Session.ResponseCount",
                               session_responses_);
  if (session_start_time_.is_null()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kWindowCloseWithoutWindowOpen);
  } else {
    base::TimeDelta open_time = base::TimeTicks::Now() - session_start_time_;
    base::UmaHistogramCustomTimes("Glic.Session.Duration", open_time,
                                  /*min=*/base::Seconds(1),
                                  /*max=*/base::Days(10), /*buckets=*/50);
  }
  session_responses_ = 0;
  session_start_time_ = base::TimeTicks();

  InputModesUsed modes_used = InputModesUsed::kNone;
  if (!inputs_modes_used_.empty()) {
    if (inputs_modes_used_.size() == 2) {
      modes_used = InputModesUsed::kTextAndAudio;
    } else {
      modes_used = inputs_modes_used_.contains(mojom::WebClientMode::kAudio)
                       ? InputModesUsed::kOnlyAudio
                       : InputModesUsed::kOnlyText;
    }
  }
  inputs_modes_used_.clear();
  base::UmaHistogramEnumeration("Glic.Session.InputModesUsed", modes_used);

  base::UmaHistogramCounts100("Glic.Session.AttachStateChanges",
                              attach_change_count_);
  attach_change_count_ = 0;

  if (base::FeatureList::IsEnabled(features::kGlicScrollTo)) {
    base::UmaHistogramCounts100("Glic.ScrollTo.SessionCount",
                                scroll_attempt_count_);
    scroll_attempt_count_ = 0;
  }

  glic_window_size_timer_.Stop();
  profile_->GetPrefs()->SetTime(prefs::kGlicWindowLastDismissedTime,
                                base::Time::Now());
}

void GlicMetrics::OnGlicScrollAttempt() {
  CHECK(base::FeatureList::IsEnabled(features::kGlicScrollTo));
  ++scroll_attempt_count_;
  if (!input_submitted_time_.is_null()) {
    scroll_input_submitted_time_ = input_submitted_time_;
    scroll_input_mode_ = input_mode_;
  }
}

void GlicMetrics::OnGlicScrollComplete(bool success) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicScrollTo));
  if (success && !scroll_input_submitted_time_.is_null()) {
    base::TimeDelta time_to_scroll =
        base::TimeTicks::Now() - scroll_input_submitted_time_;
    switch (scroll_input_mode_) {
      case mojom::WebClientMode::kAudio:
        base::UmaHistogramMediumTimes(
            "Glic.ScrollTo.UserPromptToScrollTime.Audio", time_to_scroll);
        break;
      case mojom::WebClientMode::kText:
        base::UmaHistogramMediumTimes(
            "Glic.ScrollTo.UserPromptToScrollTime.Text", time_to_scroll);
        break;
      case mojom::WebClientMode::kUnknown:
        break;
    }
  }
  scroll_input_submitted_time_ = base::TimeTicks();
  scroll_input_mode_ = mojom::WebClientMode::kUnknown;
}

void GlicMetrics::SetControllers(GlicWindowController* window_controller,
                                 GlicFocusedTabManager* tab_manager) {
  delegate_ = std::make_unique<DelegateImpl>(window_controller, tab_manager);
}

void GlicMetrics::SetDelegateForTesting(std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void GlicMetrics::DidRequestContextFromFocusedTab() {
  did_request_context_ = true;

  content::WebContents* web_contents = delegate_->GetFocusedTabData().focus();
  if (web_contents) {
    source_id_ = web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  } else {
    source_id_ = no_url_source_id_;
  }
}

void GlicMetrics::OnImpressionTimerFired() {
  if (!enabling_->IsAllowed()) {
    EntryPointStatus impression;
    if (CheckFreStatus(profile_, prefs::FreStatus::kNotStarted)) {
      // Profile not eligible, and not started FRE
      impression = EntryPointStatus::kBeforeFreNotEligible;
    } else if (CheckFreStatus(profile_, prefs::FreStatus::kIncomplete)) {
      // Profile not eligible, started but not completed FRE
      impression = EntryPointStatus::kIncompleteFreNotEligible;
    } else {
      // Profile not eligible, completed FRE
      impression = EntryPointStatus::kAfterFreNotEligible;
    }
    base::UmaHistogramEnumeration("Glic.EntryPoint.Status", impression);
    return;
  }

  // Profile eligible, has not started FRE
  if (CheckFreStatus(profile_, prefs::FreStatus::kNotStarted)) {
    base::UmaHistogramEnumeration("Glic.EntryPoint.Status",
                                  EntryPointStatus::kBeforeFreAndEligible);
    return;
  }

  // Profile eligible, started but not completed FRE
  if (CheckFreStatus(profile_, prefs::FreStatus::kIncomplete)) {
    base::UmaHistogramEnumeration("Glic.EntryPoint.Status",
                                  EntryPointStatus::kIncompleteFreAndEligible);
    return;
  }

  // Profile eligible and completed FRE
  EntryPointStatus impression;
  bool is_os_entrypoint_enabled =
      g_browser_process->local_state()->GetBoolean(prefs::kGlicLauncherEnabled);
  if (is_pinned_ && is_os_entrypoint_enabled) {
    impression = EntryPointStatus::kAfterFreBrowserAndOs;
  } else if (is_pinned_) {
    impression = EntryPointStatus::kAfterFreBrowserOnly;
  } else if (is_os_entrypoint_enabled) {
    impression = EntryPointStatus::kAfterFreOsOnly;
  } else {
    impression = EntryPointStatus::kAfterFreThreeDotOnly;
  }
  base::UmaHistogramEnumeration("Glic.EntryPoint.Status", impression);

  ui::Accelerator saved_hotkey =
      glic::GlicLauncherConfiguration::GetGlobalHotkey();
  base::UmaHistogramBoolean("Glic.OsEntrypoint.Settings.ShortcutStatus",
                            saved_hotkey != ui::Accelerator());
}

void GlicMetrics::OnGlicWindowSizeTimerFired() {
  // A 4K screen is 3840 or 4096 pixels wide and 2160 tall. Doubling this and
  // rounding up to 10000 should give a reasonable upper bound on DIPs for
  // both directions.
  gfx::Size currentSize = delegate_->GetWindowSize();
  base::UmaHistogramCounts10000("Glic.PanelWebUi.Size.Width",
                                currentSize.width());
  base::UmaHistogramCounts10000("Glic.PanelWebUi.Size.Height",
                                currentSize.height());
  base::UmaHistogramCounts10M("Glic.PanelWebUi.Size.Area",
                              currentSize.GetArea());
}

void GlicMetrics::OnMaybeEnabledAndConsentForProfileChanged() {
  bool is_enabled = enabling_->IsEnabledAndConsentForProfile(profile_);
  if (is_enabled == is_enabled_) {
    // No change, early exit.
    return;
  }
  is_enabled_ = is_enabled;
  if (is_enabled_) {
    base::RecordAction(base::UserMetricsAction("Glic.Enabled"));
  } else {
    base::RecordAction(base::UserMetricsAction("Glic.Disabled"));
  }
}

void GlicMetrics::OnPinningPrefChanged() {
  bool is_pinned =
      profile_->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);
  if (is_pinned == is_pinned_) {
    // No change, early exit.
    return;
  }
  is_pinned_ = is_pinned;
  if (is_pinned_) {
    base::RecordAction(base::UserMetricsAction("Glic.Pinned"));
  } else {
    base::RecordAction(base::UserMetricsAction("Glic.Unpinned"));
  }
}

void GlicMetrics::ResetGlicWindowPresentationTimingState() {
  show_start_time_ = base::TimeTicks();
  starting_mode_ = mojom::WebClientMode::kUnknown;
}

void GlicMetrics::OnAttachedToBrowser(AttachChangeReason reason) {
  base::UmaHistogramEnumeration("Glic.AttachedToBrowser", reason);
  if (reason != AttachChangeReason::kInit) {
    attach_change_count_++;
  }
}

void GlicMetrics::OnDetachedFromBrowser(AttachChangeReason reason) {
  base::UmaHistogramEnumeration("Glic.DetachedFromBrowser", reason);
  if (reason != AttachChangeReason::kInit) {
    attach_change_count_++;
  }
}

}  // namespace glic
