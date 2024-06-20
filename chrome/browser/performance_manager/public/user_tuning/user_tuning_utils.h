// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_

#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"

namespace performance_manager::user_tuning {

// Convenience shortcut for metrics code.
// Returns true if battery saver mode is available (via Finch) and battery saver
// mode is currently active.
bool IsRefreshRateThrottled();

// Returns whether battery saver mode should be managed by the OS
bool IsBatterySaverModeManagedByOS();

// Gets the discarded memory estimate and then calls the |result_callback| with
// the memory estimate.
void GetDiscardedMemoryEstimateForPageContext(
    resource_attribution::PageContext page_context,
    base::OnceCallback<void(uint64_t)> result_callback);

// Gets the discarded memory estimate and then calls the |result_callback| with
// the memory estimate.
using PageContextAndPmf =
    std::pair<resource_attribution::PageContext, uint64_t>;
void GetDiscardedMemoryEstimateForPageContexts(
    const std::vector<resource_attribution::PageContext>& page_contexts,
    base::OnceCallback<void(std::vector<PageContextAndPmf>)> result_callback);

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_
