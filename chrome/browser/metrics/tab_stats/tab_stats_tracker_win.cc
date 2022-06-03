// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"

namespace metrics {

void TabStatsTracker::CalculateAndRecordNativeWindowVisibilities() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BrowserList* browser_list = BrowserList::GetInstance();
  std::vector<aura::WindowTreeHost*> hosts;
  hosts.reserve(browser_list->size());

  // Get the aura::WindowTreeHost for each Chrome browser.
  for (Browser* browser : *browser_list) {
    aura::WindowTreeHost* host =
        browser->window()->GetNativeWindow()->GetHost();
    hosts.push_back(host);
  }

  // Compute native window occlusion if not using mock occlusion results.
  TabStatsTrackerDelegate::OcclusionStatusMap native_window_visibilities =
      delegate_->CallComputeNativeWindowOcclusionStatus(hosts);

  size_t num_occluded = 0;
  size_t num_visible = 0;
  size_t num_hidden = 0;

  // Determine the number of Chrome browser windows in each visibility state.
  for (auto& window_visibility_pair : native_window_visibilities) {
    aura::Window::OcclusionState visibility = window_visibility_pair.second;

    switch (visibility) {
      case aura::Window::OcclusionState::OCCLUDED:
        num_occluded++;
        break;
      case aura::Window::OcclusionState::VISIBLE:
        num_visible++;
        break;
      case aura::Window::OcclusionState::HIDDEN:
        num_hidden++;
        break;
      case aura::Window::OcclusionState::UNKNOWN:
        break;
    }
  }

  reporting_delegate_->RecordNativeWindowVisibilities(num_occluded, num_visible,
                                                      num_hidden);
}

void TabStatsTracker::UmaStatsReportingDelegate::RecordNativeWindowVisibilities(
    size_t num_occluded,
    size_t num_visible,
    size_t num_hidden) {
  UMA_HISTOGRAM_COUNTS_10000("Windows.NativeWindowVisibility.Occluded",
                             num_occluded);
  UMA_HISTOGRAM_COUNTS_10000("Windows.NativeWindowVisibility.Visible",
                             num_visible);
  UMA_HISTOGRAM_COUNTS_10000("Windows.NativeWindowVisibility.Hidden",
                             num_hidden);
}

}  // namespace metrics
