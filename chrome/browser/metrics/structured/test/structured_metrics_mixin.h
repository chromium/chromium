// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_TEST_STRUCTURED_METRICS_MIXIN_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_TEST_STRUCTURED_METRICS_MIXIN_H_

#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/structured/test/test_structured_metrics_provider.h"

// Mixin browser tests can use StructuredMetricsMixin to set up test
// environment for structured metrics recording.
//
// To use the mixin, create it as a member variable in your test, e.g.:
//
//   class MyTest : public MixinBasedInProcessBrowserTest {
//    private:
//     StructuredMetricsMixin structured_metrics_mixin_{&mixin_host_};

namespace metrics::structured {

class StructuredMetricsMixin : public InProcessBrowserTestMixin {
 public:
  explicit StructuredMetricsMixin(InProcessBrowserTestMixinHost* host);
  StructuredMetricsMixin(const StructuredMetricsMixin&) = delete;
  StructuredMetricsMixin& operator=(const StructuredMetricsMixin&) = delete;
  ~StructuredMetricsMixin() override;

  TestStructuredMetricsProvider* GetTestStructuredMetricsProvider();

  // InProcessBrowserTestMixin:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 private:
  std::unique_ptr<TestStructuredMetricsProvider>
      test_structured_metrics_provider_;
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_TEST_STRUCTURED_METRICS_MIXIN_H_
