// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LOAD_TRACKER_TEST_SUPPORT_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LOAD_TRACKER_TEST_SUPPORT_H_

#include "build/build_config.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"

class TabStripModel;

namespace content {
class WebContents;
}  // namespace content

namespace resource_coordinator {

// Helper functions for writing unittests that make use of the TabLoadTracker.
// These wait for the appropriate state transitions, returning true if they are
// observed. Waiting for transitions to explicit states can fail if the contents
// stops being tracked before reaching that state. Otherwise, these functions
// will fail by timing out.
bool WaitForTransitionToLoadingState(
    content::WebContents* contents,
    TabLoadTracker::LoadingState loading_state);
bool WaitForTransitionToUnloaded(content::WebContents* contents);
bool WaitForTransitionToLoading(content::WebContents* contents);
bool WaitForTransitionToLoaded(content::WebContents* contents);
bool WaitUntilNoLongerTracked(content::WebContents* contents);

#if !BUILDFLAG(IS_ANDROID)
// Waits until all tabs in a TabStripModel have transitioned to a given state.
bool WaitForTransitionToLoadingState(
    TabStripModel* tab_strip,
    TabLoadTracker::LoadingState loading_state);
bool WaitForTransitionToUnloaded(TabStripModel* tab_strip);
bool WaitForTransitionToLoading(TabStripModel* tab_strip);
bool WaitForTransitionToLoaded(TabStripModel* tab_strip);
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LOAD_TRACKER_TEST_SUPPORT_H_
