// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/breadcrumbs/breadcrumbs_status.h"

#include "base/feature_list.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/breadcrumbs/core/features.h"

bool BreadcrumbsStatus::IsEnabled() {
  return base::FeatureList::IsEnabled(breadcrumbs::kLogBreadcrumbs) &&
         IsMetricsAndCrashReportingEnabled();
}

bool BreadcrumbsStatus::IsMetricsAndCrashReportingEnabled() {
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}
