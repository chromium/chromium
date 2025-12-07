// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_TEST_ENVIRONMENT_H_
#define CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_TEST_ENVIRONMENT_H_

#include <memory>

#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"

namespace metrics {
class MetricsStateManager;
}  // namespace metrics

namespace variations {
class TestVariationsService;
}  // namespace variations

namespace regional_capabilities {

// This helper class makes it easier to create and use
// variations::VariationsService.
class RegionalCapabilitiesTestEnvironment {
 public:
  RegionalCapabilitiesTestEnvironment();

  ~RegionalCapabilitiesTestEnvironment();

  TestingPrefServiceSimple& pref_service();

  variations::TestVariationsService& variations_service();

 private:
  metrics::TestEnabledStateProvider enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
};

}  // namespace regional_capabilities

#endif  // CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_TEST_ENVIRONMENT_H_
