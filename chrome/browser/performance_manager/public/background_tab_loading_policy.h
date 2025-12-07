// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_BACKGROUND_TAB_LOADING_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_BACKGROUND_TAB_LOADING_POLICY_H_

#include <vector>

#include "base/functional/callback_forward.h"

namespace content {
class WebContents;
}

namespace performance_manager::policies {

// Returns true iff the BackgroundTabLoadingPolicy is installed.
bool CanScheduleLoadForRestoredTabs();

// Schedules the restored WebContents in |web_contents| to be loaded when
// appropriate. Invoked from the UI thread. Asserts that
// CanScheduleLoadForRestoredTabs() is true.
void ScheduleLoadForRestoredTabs(
    std::vector<content::WebContents*> web_contents);

// Installs a BackgroundTabLoadingPolicy that calls
// `all_restored_tabs_loaded_callback` when all tabs have been loaded in the
// Performance Manager graph. The graph must exist when this is called (for
// example in a PerformanceManagerTestHarness.)
void InstallBackgroundTabLoadingPolicyForTesting(
    base::RepeatingClosure all_restored_tabs_loaded_callback);

// Sets the maximum number of tabs that BackgroundTabLoadingPolicy will restore
// in a test. Asserts that CanScheduleLoadForRestoredTabs() is true.
void SetMaxLoadedBackgroundTabCountForTesting(size_t max_tabs_to_load);

// Sets the maximum number of simultaneous loading slots that
// BackgroundTabLoadingPolicy will use in a test. Asserts that
// CanScheduleLoadForRestoredTabs() is true.
void SetMaxSimultaneousBackgroundTabLoadsForTesting(size_t loading_slots);

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_BACKGROUND_TAB_LOADING_POLICY_H_
