// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_

#include <string>
#include <vector>

#include "base/byte_count.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"

namespace performance_manager {
class PageNode;
}

namespace performance_manager::user_tuning {

// Convenience shortcut for metrics code.
// Returns true if battery saver mode is available (via Finch) and battery saver
// mode is currently active.
bool IsRefreshRateThrottled();

// Returns whether battery saver mode should be managed by the OS
bool IsBatterySaverModeManagedByOS();

// Helper for logic to get the memory footprint estimate for a discarded page.
base::ByteCount GetDiscardedMemoryEstimateForPage(
    const performance_manager::PageNode* node);

// Returns a list of human-readable reasons why a page can't be discarded, or an
// empty list if it can be discarded.
std::vector<std::string> GetCannotDiscardReasonsForPageNode(
    const PageNode* page_node);

// Discards `page_node` if possible. This is a shortcut for the
// chrome://discards UI - most discards should use the more detailed methods in
// PageDiscardingHelper.
void DiscardPage(const performance_manager::PageNode* page_node,
                 ::mojom::LifecycleUnitDiscardReason reason,
                 bool ignore_minimum_time_in_background = false);

// Chooses and discards a PageNode if possible. This is a shortcut for the
// chrome://discards UI - most discards should use the more detailed methods in
// PageDiscardingHelper.
void DiscardAnyPage(::mojom::LifecycleUnitDiscardReason reason,
                    bool ignore_minimum_time_in_background = false);

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_
