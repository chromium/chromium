// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"

namespace content {
class WebContents;
}

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
// This must be called from the |PerformanceManager| sequence.
uint64_t GetDiscardedMemoryEstimateForPage(
    const performance_manager::PageNode* node);

// Gets the discarded memory estimate and then calls the |result_callback| with
// the memory estimate. This must be called on the UI Thread.
void GetDiscardedMemoryEstimateForWebContents(
    content::WebContents* web_contents,
    base::OnceCallback<void(uint64_t)> result_callback);

// Returns a list of human-readable reasons why a page can't be discarded, or an
// empty list if it can be discarded. This must be invoked on the PM sequence.
std::vector<std::string> GetCannotDiscardReasonsForPageNode(
    const PageNode* page_node);

// Discards `page_node`, if possible, and invokes `done_closure`. This is a
// shortcut for the chrome://discards UI - most discards should use the more
// detailed methods in PageDiscardingHelper.
void DiscardPage(const performance_manager::PageNode* page_node,
                 ::mojom::LifecycleUnitDiscardReason reason,
                 base::OnceClosure done_closure = base::DoNothing());

// Chooses and discards a PageNode, if possible, and invokes `done_closure`.
// This is a shortcut for the chrome://discards UI - most discards should use
// the more detailed methods in PageDiscardingHelper.
void DiscardAnyPage(::mojom::LifecycleUnitDiscardReason reason,
                    base::OnceClosure done_closure = base::DoNothing());

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_
