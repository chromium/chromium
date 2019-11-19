// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TAB_COUNT_METRICS_H_
#define CHROME_BROWSER_METRICS_TAB_COUNT_METRICS_H_

#include <stddef.h>

// This contains functions for creating tab count metrics that are specific
// to //chrome/browser. All bucket-related and process-independent code should
// live in //components/tab_count_metrics.
namespace tab_count_metrics {

// Returns the current number of live tabs in the browser. A tab is considered
// to be alive if it is associated with the tab UI (i.e. tabstrip), and it is
// either loading or loaded. This excludes crashed or discarded tabs.
//
// Must be called on the UI thread. This function is implemented using
// TabLoadTracker, and so it is subject to TabLoadTracker's threading rules.
// Accessing TabLoadTracker must be done from the sequence to which it is bound,
// which is meant to be the UI thread.
size_t LiveTabCount();

// Returns the current number of tabs in the browser. This includes unloaded,
// loading, and loaded tabs.
//
// Must be called on the UI thread. This function is implemented using
// TabLoadTracker, and so it is subject to TabLoadTracker's threading rules.
// Accessing TabLoadTracker must be done from the sequence to which it is bound,
// which is meant to be the UI thread.
size_t TabCount();

}  // namespace tab_count_metrics

#endif  // CHROME_BROWSER_METRICS_TAB_COUNT_METRICS_H_
