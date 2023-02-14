// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/test/structured_metrics_mixin.h"
#include "components/metrics/metrics_switches.h"

namespace metrics::structured {

StructuredMetricsMixin::StructuredMetricsMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {
  test_structured_metrics_provider_ =
      std::make_unique<TestStructuredMetricsProvider>();
}

StructuredMetricsMixin::~StructuredMetricsMixin() = default;

void StructuredMetricsMixin::SetUpCommandLine(base::CommandLine* command_line) {
  EnableMetricsRecordingOnlyForTesting(command_line);
}

TestStructuredMetricsProvider*
StructuredMetricsMixin::GetTestStructuredMetricsProvider() {
  return test_structured_metrics_provider_.get();
}

}  // namespace metrics::structured
