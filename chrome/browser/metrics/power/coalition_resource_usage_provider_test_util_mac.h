// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_COALITION_RESOURCE_USAGE_PROVIDER_TEST_UTIL_MAC_H_
#define CHROME_BROWSER_METRICS_POWER_COALITION_RESOURCE_USAGE_PROVIDER_TEST_UTIL_MAC_H_

#include "chrome/browser/metrics/power/coalition_resource_usage_provider_mac.h"

#include "components/power_metrics/resource_coalition_internal_types_mac.h"

class TestCoalitionResourceUsageProvider
    : public CoalitionResourceUsageProvider {
 public:
  TestCoalitionResourceUsageProvider();
  ~TestCoalitionResourceUsageProvider() override;

  std::unique_ptr<coalition_resource_usage> GetCoalitionResourceUsage(
      int64_t coalition_id) override;

  void SetCoalitionResourceUsage(
      std::unique_ptr<coalition_resource_usage> sample);

 private:
  std::unique_ptr<coalition_resource_usage> sample_;
};

#endif  // CHROME_BROWSER_METRICS_POWER_COALITION_RESOURCE_USAGE_PROVIDER_TEST_UTIL_MAC_H_
