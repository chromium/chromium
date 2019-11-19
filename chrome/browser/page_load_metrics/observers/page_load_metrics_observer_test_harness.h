// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TEST_HARNESS_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TEST_HARNESS_H_

#include <memory>

#include "base/macros.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_tester.h"

namespace page_load_metrics {

class PageLoadTracker;

// This class can be used to drive tests of PageLoadMetricsObservers in Chrome.
// To hook up an observer, override RegisterObservers and call
// tracker->AddObserver. This will attach the observer to all main frame
// navigations.
class PageLoadMetricsObserverTestHarness
    : public ChromeRenderViewHostTestHarness {
 public:
  // Sample URL for resource loads.
  static const char kResourceUrl[];

  PageLoadMetricsObserverTestHarness();
  ~PageLoadMetricsObserverTestHarness() override;

  void SetUp() override;

  virtual void RegisterObservers(PageLoadTracker* tracker) {}

  PageLoadMetricsObserverTester* tester() { return tester_.get(); }
  const PageLoadMetricsObserverTester* tester() const { return tester_.get(); }

 private:
  std::unique_ptr<PageLoadMetricsObserverTester> tester_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsObserverTestHarness);
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TEST_HARNESS_H_
