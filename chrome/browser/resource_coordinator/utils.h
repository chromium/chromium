// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_UTILS_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_UTILS_H_

#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"

namespace content {
class WebContents;
}  // namespace content

namespace resource_coordinator {

class TabLifecycleUnitSource;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AttemptFastKillForDiscardResult)
enum class AttemptFastKillForDiscardResult {
  kKilled = 0,
  kSkipped = 1,
  kKilledWithoutUnloadHandlers = 2,
  kKilledWithoutUnloadHandlersAndWorkers = 3,
  kMaxValue = kKilledWithoutUnloadHandlersAndWorkers,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:AttemptFastKillForDiscardResult)

// Returns the TabLifecycleUnitSource indirectly owned by g_browser_process.
TabLifecycleUnitSource* GetTabLifecycleUnitSource();

// Attempts to fast kill the process hosting the main frame of `web_contents`
// if only hosting the main frame.
void AttemptFastKillForDiscard(
    content::WebContents* web_contents,
    ::mojom::LifecycleUnitDiscardReason discard_reason);

// Discards the least important tab that supports discarding under
// `discard_reason` and returns the discarded WebContents. Uses default
// protection windows.
content::WebContents* DiscardLeastImportantTab(
    ::mojom::LifecycleUnitDiscardReason discard_reason,
    bool ignore_recent_visibility = false);

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_UTILS_H_
