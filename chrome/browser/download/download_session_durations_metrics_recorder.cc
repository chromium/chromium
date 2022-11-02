// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_session_durations_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"

namespace {
bool IsDownloadBubbleShowing() {
  Browser* browser = chrome::FindLastActive();
  return browser && browser->window() &&
         browser->window()->GetDownloadBubbleUIController() &&
         browser->window()
             ->GetDownloadBubbleUIController()
             ->GetDownloadDisplayController()
             ->IsDisplayShowingDetails();
}
}  // namespace

DownloadSessionDurationsMetricsRecorder::
    DownloadSessionDurationsMetricsRecorder()
    : session_start_(base::TimeTicks::Now()) {}

void DownloadSessionDurationsMetricsRecorder::OnSessionStarted(
    base::TimeTicks session_start) {
  base::UmaHistogramBoolean(
      "Download.Session.IsDownloadBubbleShowingWhenSessionStarts",
      IsDownloadBubbleShowing());
  // Do not reset the session start time if the bubble is showing when the last
  // session ends. Assume user is spending time on the bubble during this
  // period.
  if (!is_bubble_showing_when_session_end_) {
    session_start_ = session_start;
  }
}

void DownloadSessionDurationsMetricsRecorder::OnSessionEnded(
    base::TimeTicks session_end) {
  is_bubble_showing_when_session_end_ = IsDownloadBubbleShowing();
  base::UmaHistogramBoolean(
      "Download.Session.IsDownloadBubbleShowingWhenSessionEnds",
      is_bubble_showing_when_session_end_);
  if (!is_bubble_showing_when_session_end_) {
    // `OnSessionEnded` can be called with some delays, so we can't use
    // base::TimeTicks::Now() here.
    base::UmaHistogramLongTimes(
        "Download.Session.TotalDurationIncludingBubbleTime",
        session_end - session_start_);
  }
}
