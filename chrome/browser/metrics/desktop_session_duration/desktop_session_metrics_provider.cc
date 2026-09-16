// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_session_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "components/activity_reporter/activity_reporter.h"

namespace metrics {

namespace {

class DesktopSessionMetricsProvider : public MetricsProvider {
 public:
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* /*uma_proto*/) override {
    const bool in_session = DesktopSessionDurationTracker::Get()->in_session();
    if (DesktopSessionDurationTracker::IsInitialized()) {
      // TODO(crbug.com/560014755): Stop reporting Session.IsActive.
      base::UmaHistogramBoolean("Session.IsActive", in_session);
      base::UmaHistogramBoolean("Session.IsActive2", in_session);
    }
    if (in_session) {
      g_browser_process->activity_reporter()->ReportActive();
    }
  }
};

}  // namespace

std::unique_ptr<MetricsProvider> CreateDesktopSessionMetricsProvider() {
  return std::make_unique<DesktopSessionMetricsProvider>();
}

}  // namespace metrics
