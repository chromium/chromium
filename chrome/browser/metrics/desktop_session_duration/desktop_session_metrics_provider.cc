// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_session_metrics_provider.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"

namespace metrics {

namespace {

class DesktopSessionMetricsProvider : public MetricsProvider {
 public:
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* /*uma_proto*/) override {
    if (DesktopSessionDurationTracker::IsInitialized()) {
      UMA_HISTOGRAM_BOOLEAN("Session.IsActive",
                            DesktopSessionDurationTracker::Get()->in_session());
    }
  }
};

}  // namespace

std::unique_ptr<MetricsProvider> CreateDesktopSessionMetricsProvider() {
  return std::make_unique<DesktopSessionMetricsProvider>();
}

}  // namespace metrics
