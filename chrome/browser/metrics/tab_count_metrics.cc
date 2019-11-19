// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_count_metrics.h"

#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "content/public/browser/browser_thread.h"

namespace tab_count_metrics {

size_t LiveTabCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* tab_load_tracker = resource_coordinator::TabLoadTracker::Get();
  return tab_load_tracker->GetLoadingUiTabCount() +
         tab_load_tracker->GetLoadedUiTabCount();
}

size_t TabCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* tab_load_tracker = resource_coordinator::TabLoadTracker::Get();
  return tab_load_tracker->GetUiTabCount();
}

}  // namespace tab_count_metrics
