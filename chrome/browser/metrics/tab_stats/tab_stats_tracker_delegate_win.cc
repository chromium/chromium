// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats/tab_stats_tracker_delegate.h"

#include "ui/aura_extra/window_occlusion_win.h"

TabStatsTrackerDelegate::OcclusionStatusMap
TabStatsTrackerDelegate::CallComputeNativeWindowOcclusionStatus(
    std::vector<aura::WindowTreeHost*> hosts) {
  return aura_extra::ComputeNativeWindowOcclusionStatus(hosts);
}
