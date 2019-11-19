// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_WINDOW_FEATURES_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_WINDOW_FEATURES_H_

#include <string>

#include "chrome/browser/resource_coordinator/tab_metrics_event.pb.h"
#include "components/sessions/core/session_id.h"

namespace tab_ranker {

// Window features used for logging a Tab Ranker example to UKM or calculating a
// Tab Ranker score.
struct WindowFeatures {
  WindowFeatures(metrics::WindowMetricsEvent::Type type,
                 metrics::WindowMetricsEvent::ShowState show_state,
                 bool is_active,
                 int tab_count);

  ~WindowFeatures() = default;

  bool operator==(const WindowFeatures& other) const;
  bool operator!=(const WindowFeatures& other) const;

  const metrics::WindowMetricsEvent::Type type;
  metrics::WindowMetricsEvent::ShowState show_state =
      metrics::WindowMetricsEvent::SHOW_STATE_UNKNOWN;

  // True if this is the active (frontmost) window.
  bool is_active = false;

  // Number of tabs in the tab strip.
  int tab_count = 0;
};

}  // namespace tab_ranker

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_WINDOW_FEATURES_H_
