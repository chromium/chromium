// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/performance_manager/public/performance_manager_main_thread_observer.h"

namespace performance_manager {

// This enum matches "StabilityPageLoadType" in enums.xml. The ordering
// of values must match the ordering of values in the NavigationType enum.
enum class LoadType {
  kVisibleTabBase = 0,
  kVisibleTabMainFrameDifferentDocument = 0,
  kVisibleTabSubFrameDifferentDocument = 1,
  kVisibleTabMainFrameSameDocument = 2,
  kVisibleTabSubFrameSameDocument = 3,
  kVisibleTabNoCommit = 4,

  kHiddenTabBase = 5,
  kHiddenTabMainFrameDifferentDocument = 5,
  kHiddenTabSubFrameDifferentDocument = 6,
  kHiddenTabMainFrameSameDocument = 7,
  kHiddenTabSubFrameSameDocument = 8,
  kHiddenTabNoCommit = 9,

  kPrerenderBase = 10,
  kPrerenderMainFrameDifferentDocument = 10,
  kPrerenderSubFrameDifferentDocument = 11,
  kPrerenderMainFrameSameDocument = 12,
  kPrerenderSubFrameSameDocument = 13,
  kPrerenderNoCommit = 14,

  kExtension = 15,
  kDevTools = 16,

  kUnknown = 17,

  kMaxValue = kUnknown,
};

// PageLoadMetricsObserver records detailed metrics to explain what is included
// in the "Total Pageloads" presented on stability dashboards.
class PageLoadMetricsObserver
    : public PerformanceManagerMainThreadObserverDefaultImpl {
 public:
  PageLoadMetricsObserver();
  ~PageLoadMetricsObserver() override;
  PageLoadMetricsObserver(const PageLoadMetricsObserver& other) = delete;
  PageLoadMetricsObserver& operator=(const PageLoadMetricsObserver&) = delete;

  // PerformanceManagerMainThreadObserver:
  void OnPageNodeCreatedForWebContents(
      content::WebContents* web_contents) override;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_H_
