// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_

#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager::user_tuning {

// Convenience shortcut for metrics code.
// Returns true if battery saver mode is available (via Finch) and battery saver
// mode is currently active.
bool IsRefreshRateThrottled();

// Returns whether battery saver mode should be managed by the OS
bool IsBatterySaverModeManagedByOS();

// Helper for logic to get the memory footprint estimate for a discarded page.
// This must be called from the |PerformanceManager| sequence.
uint64_t GetDiscardedMemoryEstimateForPage(
    const performance_manager::PageNode* node);

// Gets the discarded memory estimate and then calls the |result_callback| with
// the memory estimate. This must be called on the UI Thread.
void GetDiscardedMemoryEstimateForWebContents(
    content::WebContents* web_contents,
    base::OnceCallback<void(uint64_t)> result_callback);

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_
