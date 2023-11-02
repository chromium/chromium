// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_session_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"

namespace metrics {

namespace {

class DesktopSessionMetricsProvider : public MetricsProvider {
 public:
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* /*uma_proto*/) override {
    if (DesktopSessionDurationTracker::IsInitialized()) {
      base::UmaHistogramBoolean(
          "Session.IsActive",
          DesktopSessionDurationTracker::Get()->in_session());
    }
  }
};

}  // namespace

std::unique_ptr<MetricsProvider> CreateDesktopSessionMetricsProvider() {
  return std::make_unique<DesktopSessionMetricsProvider>();
}

}  // namespace metrics
