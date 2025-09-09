// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_UTILS_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_UTILS_H_

#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"

namespace content {
class WebContents;
}  // namespace content

namespace resource_coordinator {

class TabLifecycleUnitSource;

// Returns the TabLifecycleUnitSource indirectly owned by g_browser_process.
TabLifecycleUnitSource* GetTabLifecycleUnitSource();

// Attempts to fast kill the process hosting the main frame of `web_contents`
// if only hosting the main frame.
void AttemptFastKillForDiscard(
    content::WebContents* web_contents,
    ::mojom::LifecycleUnitDiscardReason discard_reason);

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_UTILS_H_
