// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BREADCRUMBS_BREADCRUMBS_STATUS_H_
#define CHROME_BROWSER_BREADCRUMBS_BREADCRUMBS_STATUS_H_

// Returns whether breadcrumbs crash logging is enabled. When enabled,
// breadcrumbs appends a short history of events to crash reports. These can be
// used to diagnose the cause of the attached crash by providing context for the
// crash.
// Breadcrumb events include:
// * user-triggered actions listed in tools/metrics/actions.xml
// * per-tab actions, e.g., when navigation starts/finishes
// * per-browser actions, e.g., number of tabs opened/closed/moved
// * memory pressure
// * startup/shutdown
class BreadcrumbsStatus {
 public:
  // Returns true if breadcrumbs logging is enabled. This is only the case if
  // both the feature is enabled and the user has consented to metrics
  // recording. If metrics consent is disabled, breadcrumbs will not be
  // re-enabled until consent is given _and_ the browser restarts.
  static bool IsEnabled();

 private:
  // Returns true if the user has consented to metrics reporting. Breadcrumbs
  // should only be appended to crash reports if this is true, as they use
  // information from UMA.
  static bool IsMetricsAndCrashReportingEnabled();
};

#endif  // CHROME_BROWSER_BREADCRUMBS_BREADCRUMBS_STATUS_H_
