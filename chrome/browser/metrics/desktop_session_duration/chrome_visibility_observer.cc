// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/chrome_visibility_observer.h"

#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

namespace metrics {

ChromeVisibilityObserver::ChromeVisibilityObserver() {
  BrowserList::AddObserver(this);
  InitVisibilityGapTimeout();
}

ChromeVisibilityObserver::~ChromeVisibilityObserver() {
  BrowserList::RemoveObserver(this);
}

void ChromeVisibilityObserver::SendVisibilityChangeEvent(
    bool active,
    base::TimeDelta time_ago) {
  DesktopSessionDurationTracker::Get()->OnVisibilityChanged(active, time_ago);
}

void ChromeVisibilityObserver::CancelVisibilityChange() {
  weak_factory_.InvalidateWeakPtrs();
}

void ChromeVisibilityObserver::OnBrowserSetLastActive(Browser* browser) {
  if (weak_factory_.HasWeakPtrs())
    CancelVisibilityChange();
  else
    SendVisibilityChangeEvent(true, base::TimeDelta());
}

void ChromeVisibilityObserver::OnBrowserNoLongerActive(Browser* browser) {
  if (visibility_gap_timeout_.InMicroseconds() == 0) {
    SendVisibilityChangeEvent(false, base::TimeDelta());
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ChromeVisibilityObserver::SendVisibilityChangeEvent,
                       weak_factory_.GetWeakPtr(), false,
                       visibility_gap_timeout_),
        visibility_gap_timeout_);
  }
}

void ChromeVisibilityObserver::OnBrowserRemoved(Browser* browser) {
  // If there are no browser instances left then we should notify that browser
  // is not visible anymore immediately without waiting.
  if (BrowserList::GetInstance()->empty()) {
    CancelVisibilityChange();
    SendVisibilityChangeEvent(false, base::TimeDelta());
  }
}

void ChromeVisibilityObserver::InitVisibilityGapTimeout() {
  const int kDefaultVisibilityGapTimeout = 3;

  int timeout_seconds = kDefaultVisibilityGapTimeout;
  std::string param_value = base::GetFieldTrialParamValue(
      "DesktopSessionDuration", "visibility_gap_timeout");
  if (!param_value.empty())
    base::StringToInt(param_value, &timeout_seconds);

  visibility_gap_timeout_ = base::Seconds(timeout_seconds);
}

void ChromeVisibilityObserver::SetVisibilityGapTimeoutForTesting(
    base::TimeDelta timeout) {
  visibility_gap_timeout_ = timeout;
}

}  // namespace metrics
