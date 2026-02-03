// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_SERVICE_ACCESSOR_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_SERVICE_ACCESSOR_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_accessor.h"
#include "components/variations/synthetic_trial_registry.h"

namespace android_webview {

// This class limits and documents access to metrics service helper methods.
// Since these methods are private, each user has to be explicitly declared
// as a 'friend' below.
class AwMetricsServiceAccessor : public metrics::MetricsServiceAccessor {
 private:
  friend class AwBrowserMainParts;
  friend class AwSettings;
  friend class AwPrefetchManager;
  friend class AwMetricsTestBase;
  friend class AwMetricsServiceAccessorTest;
  FRIEND_TEST_ALL_PREFIXES(AwMetricsServiceAccessorTest,
                           RegisterExternalExperimentUpdatesCorrectly);
  FRIEND_TEST_ALL_PREFIXES(AwMetricsServiceAccessorTest,
                           RegisterExternalExperimentOrderingAgnostic);

  static void RegisterExternalExperiment(
      const std::vector<int>& experiment_ids);

  static void ClearAllExternalExperimentsForTesting();
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_SERVICE_ACCESSOR_H_
