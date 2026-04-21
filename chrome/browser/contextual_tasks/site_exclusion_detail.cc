// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/site_exclusion_detail.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"

namespace contextual_tasks {

void SiteExclusionDetail::RecordActiveTabMetrics() {
  CHECK_LE(tabs_checked, 1);
  base::UmaHistogramCounts100(
      "ContextualTasks.Context.SiteExclusions.ActiveTab.ExclusionsChecked",
      exclusions_checked);
  base::UmaHistogramBoolean(
      "ContextualTasks.Context.SiteExclusions.ActiveTab.Filtered",
      tabs_filtered == 1);
}

void SiteExclusionDetail::RecordAllTabsMetrics() {
  base::UmaHistogramTimes(
      "ContextualTasks.Context.SiteExclusions.AllTabs.CheckingDuration",
      duration);
  base::UmaHistogramCounts100(
      "ContextualTasks.Context.SiteExclusions.AllTabs.TabsChecked",
      tabs_checked);
  base::UmaHistogramCounts100(
      "ContextualTasks.Context.SiteExclusions.AllTabs.TabsFiltered",
      tabs_filtered);
}

}  // namespace contextual_tasks
