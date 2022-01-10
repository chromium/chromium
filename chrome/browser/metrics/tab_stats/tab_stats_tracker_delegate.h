// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_TRACKER_DELEGATE_H_
#define CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_TRACKER_DELEGATE_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

class TabStatsTrackerDelegate {
 public:
  TabStatsTrackerDelegate() {}
  virtual ~TabStatsTrackerDelegate() {}

#if BUILDFLAG(IS_WIN)
  using OcclusionStatusMap =
      base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState>;

  virtual OcclusionStatusMap CallComputeNativeWindowOcclusionStatus(
      std::vector<aura::WindowTreeHost*> hosts);
#endif  // BUILDFLAG(IS_WIN)
};

#endif  // CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_TRACKER_DELEGATE_H_
