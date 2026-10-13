// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_ACTIONABLE_MARK_OBSERVER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_ACTIONABLE_MARK_OBSERVER_H_

#include "base/functional/callback.h"
#include "base/time/time.h"

namespace content {
class WebContents;
}  // namespace content

namespace glic {

// Runs with the absolute time of the guest's actionability mark.
using GlicActionableMarkCallback =
    base::RepeatingCallback<void(base::TimeTicks)>;

// Attaches a `page_load_metrics::MetricsWebContentsObserver` to
// `guest_contents` that watches the custom `performance.mark()` timings
// reported by each guest navigation and runs `callback` with the time of the
// first mark named "gemini.interactive" (the web client's signal that it has
// become actionable). Must be called before the guest's first navigation
// starts. Does nothing if `guest_contents` already has a
// `MetricsWebContentsObserver`.
void ObserveGlicGuestActionableMark(content::WebContents& guest_contents,
                                    GlicActionableMarkCallback callback);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_GUEST_ACTIONABLE_MARK_OBSERVER_H_
