// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_fre_controller.h"
#include "chrome/browser/glic/glic_window_controller.h"

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
  kAfterFreGlicEnabled = 1,
  kAfterFreGlicDisabled = 2,
  kMaxValue = kAfterFreGlicDisabled,
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

GlicMetrics::GlicMetrics(Profile* profile) : profile_(profile) {
  impression_timer_.Start(
      FROM_HERE, base::Minutes(15),
      base::BindRepeating(&GlicMetrics::OnImpressionTimerFired,
                          base::Unretained(this)));
}
GlicMetrics::~GlicMetrics() = default;

void GlicMetrics::OnUserInputSubmitted(mojom::WebClientMode mode) {
  base::RecordAction(base::UserMetricsAction("GlicResponseInputSubmit"));
  input_submitted_time_ = base::TimeTicks::Now();
  input_mode_ = mode;
}

void GlicMetrics::OnResponseStarted() {
  base::RecordAction(base::UserMetricsAction("GlicResponseStart"));

  // It doesn't make sense to record response start without input submission.
  if (input_submitted_time_.is_null()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kResponseStartWithoutInput);
    return;
  }

  if (!controller_->IsShowing()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kResponseStartWhileHidingOrHidden);
    return;
  }

  response_started_time_ = base::TimeTicks::Now();
  base::UmaHistogramMediumTimes("Glic.Response.StartTime",
                                response_started_time_ - input_submitted_time_);
  switch (input_mode_) {
    case mojom::WebClientMode::kUnknown:
      base::UmaHistogramMediumTimes(
          "Glic.Response.StartTime.InputMode.Unknown",
          response_started_time_ - input_submitted_time_);
      break;
    case mojom::WebClientMode::kText:
      base::UmaHistogramMediumTimes(
          "Glic.Response.StartTime.InputMode.Text",
          response_started_time_ - input_submitted_time_);
      break;
    case mojom::WebClientMode::kAudio:
      base::UmaHistogramMediumTimes(
          "Glic.Response.StartTime.InputMode.Audio",
          response_started_time_ - input_submitted_time_);
      break;
  }
  base::RecordAction(base::UserMetricsAction("GlicResponse"));
  ++session_responses_;

  // More detailed metrics.
  bool attached = controller_->IsAttached();
  base::UmaHistogramBoolean("Glic.Response.Attached", attached);
  base::UmaHistogramEnumeration("Glic.Response.InvocationSource",
                                invocation_source_);
  base::UmaHistogramEnumeration("Glic.Response.InputMode", input_mode_);
  base::UmaHistogramEnumeration(
      "Glic.Response.Segmentation",
      GetResponseSegmentation(attached, input_mode_, invocation_source_));
}

void GlicMetrics::OnResponseStopped() {
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
  response_started_time_ = base::TimeTicks();
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
}

void GlicMetrics::OnGlicWindowClose() {
  base::RecordAction(base::UserMetricsAction("GlicSessionEnd"));
  base::UmaHistogramCounts1000("Glic.Session.ResponseCount",
                               session_responses_);
  if (session_start_time_.is_null()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kWindowCloseWithoutWindowOpen);
  } else {
    base::TimeDelta open_time = base::TimeTicks() - session_start_time_;
    base::UmaHistogramCustomTimes("Glic.Session.Duration", open_time,
                                  /*min=*/base::Seconds(1),
                                  /*max=*/base::Days(10), /*buckets=*/50);
  }
  session_responses_ = 0;
  session_start_time_ = base::TimeTicks();
}

void GlicMetrics::SetWindowController(GlicWindowController* controller) {
  controller_ = controller;
}

void GlicMetrics::OnImpressionTimerFired() {
  bool passed_fre = !controller_->fre_controller()->ShouldShowFreDialog();
  EntryPointImpression impression;
  if (passed_fre) {
    bool glic_enabled = GlicEnabling::IsEnabledForProfile(profile_);
    if (glic_enabled) {
      impression = EntryPointImpression::kAfterFreGlicEnabled;
    } else {
      impression = EntryPointImpression::kAfterFreGlicDisabled;
    }
  } else {
    impression = EntryPointImpression::kBeforeFre;
  }
  base::UmaHistogramEnumeration("Glic.EntryPoint.Impression", impression);
}

}  // namespace glic
