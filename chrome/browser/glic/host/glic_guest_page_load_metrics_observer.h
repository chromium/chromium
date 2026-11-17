// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_PAGE_LOAD_METRICS_OBSERVER_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/time/time.h"

namespace content {
class WebContents;
}  // namespace content

namespace glic {

// Runs with the custom `performance.mark()` name and its absolute timestamp
// (`navigation_start + mark.start_time`).
using GlicGuestTimingMarkCallback =
    base::RepeatingCallback<void(std::string_view mark_name,
                                 base::TimeTicks mark_time)>;

// Attaches a `page_load_metrics::MetricsWebContentsObserver` to
// `guest_contents` that watches custom `performance.mark()` timings reported by
// each primary main-frame guest navigation and runs `callback` (which must be
// non-null) for each observed mark. Must be called before the guest's first
// navigation starts. Does nothing if `guest_contents` already has a
// `MetricsWebContentsObserver`.
void ObserveGlicGuestPageLoadMetrics(content::WebContents& guest_contents,
                                     GlicGuestTimingMarkCallback callback);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_PAGE_LOAD_METRICS_OBSERVER_H_
