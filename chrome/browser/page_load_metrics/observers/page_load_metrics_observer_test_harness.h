// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TEST_HARNESS_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TEST_HARNESS_H_

#include <memory>

#include "base/test/scoped_feature_list.h"
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

  // Construct a PageLoadMetricsObserverTestHarness with zero or more arguments
  // passed to ChromeRenderViewHostTestHarness.
  template <typename... TaskEnvironmentTraits>
  explicit PageLoadMetricsObserverTestHarness(TaskEnvironmentTraits&&... traits)
      : ChromeRenderViewHostTestHarness(
            std::forward<TaskEnvironmentTraits>(traits)...) {
    InitializeFeatureList();
  }

  PageLoadMetricsObserverTestHarness(
      const PageLoadMetricsObserverTestHarness&) = delete;
  PageLoadMetricsObserverTestHarness& operator=(
      const PageLoadMetricsObserverTestHarness&) = delete;

  ~PageLoadMetricsObserverTestHarness() override;

  void SetUp() override;

  virtual void RegisterObservers(PageLoadTracker* tracker) {}

  PageLoadMetricsObserverTester* tester() { return tester_.get(); }
  const PageLoadMetricsObserverTester* tester() const { return tester_.get(); }

 protected:
  virtual bool IsNonTabWebUI() const;

 private:
  void InitializeFeatureList();

  std::unique_ptr<PageLoadMetricsObserverTester> tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TEST_HARNESS_H_
