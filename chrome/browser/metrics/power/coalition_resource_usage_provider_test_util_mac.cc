// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/coalition_resource_usage_provider_test_util_mac.h"

TestCoalitionResourceUsageProvider::TestCoalitionResourceUsageProvider() {
  // Used a constant timebase to facilitate writing expectations.
  timebase_ = {1, 1};
  // Tests are typically run from a Terminal and are therefore not alone in
  // their coalition.
  ignore_not_alone_for_testing_ = true;
}

TestCoalitionResourceUsageProvider::~TestCoalitionResourceUsageProvider() =
    default;

std::unique_ptr<coalition_resource_usage>
TestCoalitionResourceUsageProvider::GetCoalitionResourceUsage(
    int64_t coalition_id) {
  return std::move(sample_);
}

void TestCoalitionResourceUsageProvider::SetCoalitionResourceUsage(
    std::unique_ptr<coalition_resource_usage> sample) {
  sample_ = std::move(sample);
}
