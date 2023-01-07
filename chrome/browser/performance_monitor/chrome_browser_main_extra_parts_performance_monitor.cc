// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/chrome_browser_main_extra_parts_performance_monitor.h"

#include "chrome/browser/performance_monitor/system_monitor.h"

ChromeBrowserMainExtraPartsPerformanceMonitor::
    ChromeBrowserMainExtraPartsPerformanceMonitor() = default;

ChromeBrowserMainExtraPartsPerformanceMonitor::
    ~ChromeBrowserMainExtraPartsPerformanceMonitor() = default;

void ChromeBrowserMainExtraPartsPerformanceMonitor::
    PostCreateMainMessageLoop() {
  system_monitor_ = performance_monitor::SystemMonitor::Create();
}
