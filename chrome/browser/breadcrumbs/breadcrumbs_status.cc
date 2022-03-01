// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/breadcrumbs/breadcrumbs_status.h"

#include "base/feature_list.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/breadcrumbs/core/features.h"

bool BreadcrumbsStatus::IsEnabled() {
  static bool is_enabled = true;
  // If `is_enabled` is ever false, it will stay false to prevent parts of
  // breadcrumbs trying to run (e.g., helpers on new tabs, browser agents on new
  // windows) when others are uninitialized (e.g., the browser process logger).
  // In other words, breadcrumbs is only enabled if it has been continuously
  // enabled since Chrome started.
  if (!is_enabled)
    return is_enabled;
  is_enabled = base::FeatureList::IsEnabled(breadcrumbs::kLogBreadcrumbs) &&
               IsMetricsAndCrashReportingEnabled();
  return is_enabled;
}

bool BreadcrumbsStatus::IsMetricsAndCrashReportingEnabled() {
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}
