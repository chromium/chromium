// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_PARTS_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_PARTS_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_memory_metrics_reporter.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#endif

namespace resource_coordinator {

#if defined(OS_ANDROID)
class TabManager;
class TabLifecycleUnitSource;
#endif

// Contains the various parts of the resource coordinator. There should be only
// one instance of this class created early during the initialization of the
// browser process.
class ResourceCoordinatorParts {
 public:
  ResourceCoordinatorParts();
  ~ResourceCoordinatorParts();

  TabMemoryMetricsReporter* tab_memory_metrics_reporter() {
    if (!tab_memory_metrics_reporter_.get())
      tab_memory_metrics_reporter_.reset(new TabMemoryMetricsReporter());
    return tab_memory_metrics_reporter_.get();
  }

  TabLoadTracker* tab_load_tracker() { return &tab_load_tracker_; }

  TabManager* tab_manager() {
#if defined(OS_ANDROID)
    return nullptr;
#else
    return &tab_manager_;
#endif  // defined(OS_ANDROID)
  }

  TabLifecycleUnitSource* tab_lifecycle_unit_source() {
#if defined(OS_ANDROID)
    return nullptr;
#else
    return &tab_lifecycle_unit_source_;
#endif  // defined(OS_ANDROID)
  }

 private:
  // This should be declared before |tab_memory_metrics_reporter_| and
  // |tab_manager_| as they both depend on this at shutdown.
  TabLoadTracker tab_load_tracker_;

  // Created on demand the first time it's being accessed.
  std::unique_ptr<TabMemoryMetricsReporter> tab_memory_metrics_reporter_;

#if !defined(OS_ANDROID)
  // Any change to this #ifdef must be reflected as well in
  // chrome/browser/resource_coordinator/tab_manager_browsertest.cc
  //
  // The order of these 2 members matters, TabLifecycleUnitSource must be
  // deleted before TabManager because it has a raw pointer to a UsageClock
  // owned by TabManager.
  TabManager tab_manager_;
  TabLifecycleUnitSource tab_lifecycle_unit_source_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorParts);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_PARTS_H__
