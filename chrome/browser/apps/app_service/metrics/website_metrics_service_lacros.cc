// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/website_metrics_service_lacros.h"

#include "base/time/time.h"

namespace apps {

namespace {

// Interval for reporting noisy AppKM events.
constexpr base::TimeDelta kNoisyAppKMReportInterval = base::Hours(2);

// Check for app usage time, input event each 5 minutes.
constexpr base::TimeDelta kFiveMinutes = base::Minutes(5);

}  // namespace

WebsiteMetricsServiceLacros::WebsiteMetricsServiceLacros() = default;

WebsiteMetricsServiceLacros::~WebsiteMetricsServiceLacros() = default;

void WebsiteMetricsServiceLacros::Start() {
  // TODO(crbug.com/1334173): Create WebsiteMetrics to record website metrics.

  // Check every `kFiveMinutes` to record websites usage time.
  five_minutes_timer_.Start(FROM_HERE, kFiveMinutes, this,
                            &WebsiteMetricsServiceLacros::CheckForFiveMinutes);

  // Check every `kNoisyAppKMReportInterval` to report noisy AppKM events.
  noisy_appkm_reporting_interval_timer_.Start(
      FROM_HERE, kNoisyAppKMReportInterval, this,
      &WebsiteMetricsServiceLacros::CheckForNoisyAppKMReportingInterval);
}

void WebsiteMetricsServiceLacros::CheckForFiveMinutes() {
  // TODO(crbug.com/1334173): Call WebsiteMetrics OnFiveMinutes to record
  // website metrics in the user pref.
}

void WebsiteMetricsServiceLacros::CheckForNoisyAppKMReportingInterval() {
  // TODO(crbug.com/1334173): Call WebsiteMetrics OnTwoHours to log website
  // metrics UKM.
}

}  // namespace apps
