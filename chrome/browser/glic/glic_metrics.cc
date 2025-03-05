// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_focused_tab_manager.h"
#include "chrome/browser/glic/glic_fre_controller.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace glic {

namespace {

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

// LINT.IfChange(EntryPointImpression)
enum class EntryPointImpression {
  kBeforeFre = 0,
  kAfterFreBrowserOnly = 1,
  kAfterFreOsOnly = 2,
  kAfterFreEnabled = 3,
  kAfterFreDisabled = 4,
  kMaxValue = kAfterFreDisabled,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicEntryPointImpression)

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
  kChroMenuAttachedText = 29,
  kChroMenuAttachedAudio = 30,
  kChroMenuDetachedText = 31,
  kChroMenuDetachedAudio = 32,
  kMaxValue = kChroMenuDetachedAudio,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicResponseSegmentation)

ResponseSegmentation GetResponseSegmentation(bool attached,
                                             mojom::WebClientMode mode,
                                             InvocationSource source) {
  if (mode == mojom::WebClientMode::kUnknown) {
    return ResponseSegmentation::kUnknown;
  }
  // Entries start at 1 since 0 is kUnknown.
  int entry = 1;
  // Text mode is 0 mod 2, audio mode is 1 mod 2.
  if (mode == mojom::WebClientMode::kAudio) {
    entry += 1;
  }
  // Attached entries are 0,1 mod 4, detached entries are 2,3 mod 4.
  if (!attached) {
    entry += 2;
  }
  switch (source) {
    case InvocationSource::kOsButton:
      break;
    case InvocationSource::kOsButtonMenu:
      entry += 4;
      break;
    case InvocationSource::kOsHotkey:
      entry += 8;
      break;
    case InvocationSource::kTopChromeButton:
      entry += 12;
      break;
    case InvocationSource::kFre:
      entry += 16;
      break;
    case InvocationSource::kProfilePicker:
      entry += 20;
      break;
    case InvocationSource::kNudge:
      entry += 24;
      break;
    case InvocationSource::kChroMenu:
      entry += 28;
      break;
  }
  return static_cast<ResponseSegmentation>(entry);
}

}  // namespace

GlicMetrics::GlicMetrics(Profile* profile, GlicEnabling* enabling)
    : profile_(profile), enabling_(enabling) {
  impression_timer_.Start(
      FROM_HERE, base::Minutes(15),
      base::BindRepeating(&GlicMetrics::OnImpressionTimerFired,
                          base::Unretained(this)));
  no_url_source_id_ = ukm::NoURLSourceId();
  source_id_ = no_url_source_id_;

  is_enabled_ = enabling_->IsEnabled();
  subscriptions_.push_back(enabling_->RegisterEnableChanged(base::BindRepeating(
      &GlicMetrics::OnEnabledChanged, base::Unretained(this))));

  is_pinned_ = profile_->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(prefs::kGlicPinnedToTabstrip,
                      base::BindRepeating(&GlicMetrics::OnPinningPrefChanged,
                                          base::Unretained(this)));
}
GlicMetrics::~GlicMetrics() = default;

void GlicMetrics::OnUserInputSubmitted(mojom::WebClientMode mode) {
  base::RecordAction(base::UserMetricsAction("GlicResponseInputSubmit"));
  input_submitted_time_ = base::TimeTicks::Now();
  input_mode_ = mode;
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

  if (!window_controller_->IsShowing()) {
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
  bool attached = window_controller_->IsAttached();
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

void GlicMetrics::OnGlicWindowOpen(bool attached, InvocationSource source) {
  base::RecordAction(base::UserMetricsAction("GlicSessionBegin"));
  session_start_time_ = base::TimeTicks::Now();
  invocation_source_ = source;
  base::UmaHistogramBoolean("Glic.Session.Open.Attached", attached);
  base::UmaHistogramEnumeration("Glic.Session.Open.InvocationSource", source);

  ukm::builders::Glic_WindowOpen(source_id_)
      .SetAttached(attached)
      .SetInvocationSource(static_cast<int64_t>(source))
      .Record(ukm::UkmRecorder::Get());
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
}

void GlicMetrics::SetControllers(GlicWindowController* window_controller,
                                 GlicFocusedTabManager* tab_manager) {
  window_controller_ = window_controller;
  tab_manager_ = tab_manager;
}

void GlicMetrics::DidRequestContextFromFocusedTab() {
  did_request_context_ = true;

  content::WebContents* web_contents =
      tab_manager_->GetFocusedTabData().focused_tab_contents.get();
  if (web_contents) {
    source_id_ = web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  } else {
    source_id_ = no_url_source_id_;
  }
}

void GlicMetrics::OnImpressionTimerFired() {
  if (window_controller_->fre_controller()->ShouldShowFreDialog()) {
    base::UmaHistogramEnumeration("Glic.EntryPoint.Impression",
                                  EntryPointImpression::kBeforeFre);
    return;
  }
  if (!is_enabled_) {
    base::UmaHistogramEnumeration("Glic.EntryPoint.Impression",
                                  EntryPointImpression::kAfterFreDisabled);
    return;
  }

  EntryPointImpression impression;
  bool is_os_entrypoint_enabled =
      g_browser_process->local_state()->GetBoolean(prefs::kGlicLauncherEnabled);
  if (is_pinned_ && is_os_entrypoint_enabled) {
    impression = EntryPointImpression::kAfterFreEnabled;
  } else if (is_pinned_) {
    impression = EntryPointImpression::kAfterFreBrowserOnly;
  } else if (is_os_entrypoint_enabled) {
    impression = EntryPointImpression::kAfterFreOsOnly;
  } else {
    impression = EntryPointImpression::kAfterFreDisabled;
  }
  base::UmaHistogramEnumeration("Glic.EntryPoint.Impression", impression);
}

void GlicMetrics::OnEnabledChanged() {
  bool is_enabled = enabling_->IsEnabled();
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

}  // namespace glic
