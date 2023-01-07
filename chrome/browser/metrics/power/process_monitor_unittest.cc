// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/process_monitor.h"

#include <utility>

#include "content/public/browser/child_process_data.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "services/network/public/mojom/network_service.mojom.h"
TEST(ProcessMonitorTest, MonitoredProcessType) {
  const std::pair<int, MonitoredProcessType> kExpectations[] = {
      {content::PROCESS_TYPE_GPU, MonitoredProcessType::kGpu},
      {content::PROCESS_TYPE_UTILITY, MonitoredProcessType::kUtility},
      {content::PROCESS_TYPE_ZYGOTE, MonitoredProcessType::kOther},
  };

  for (const auto& [process_type, expectation] : kExpectations) {
    content::ChildProcessData data(process_type);
    // data.process_type = process_type;
    EXPECT_EQ(GetMonitoredProcessTypeForNonRendererChildProcessForTesting(data),
              expectation);
  }

  // Special case for network process.
  content::ChildProcessData data(content::PROCESS_TYPE_UTILITY);
  data.metrics_name = network::mojom::NetworkService::Name_;
  EXPECT_EQ(GetMonitoredProcessTypeForNonRendererChildProcessForTesting(data),
            MonitoredProcessType::kNetwork);
}
