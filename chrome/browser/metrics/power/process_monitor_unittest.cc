// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/process_monitor.h"

#include <utility>

#include "content/public/browser/child_process_data.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "services/network/public/mojom/network_service.mojom.h"
TEST(ProcessMonitorTest, MonitoredProcessType) {
  const std::pair<int, ProcessInfo::Key> kExpectations[] = {
      {content::PROCESS_TYPE_GPU, {MonitoredProcessType::kGpu, std::nullopt}},
      // The ChildProcessData's metrics_name isn't optional, so the subtype is
      // ""
      {content::PROCESS_TYPE_UTILITY, {MonitoredProcessType::kUtility, ""}},
      {content::PROCESS_TYPE_ZYGOTE,
       {MonitoredProcessType::kOther, std::nullopt}},
  };

  for (const auto& [process_type, expectation] : kExpectations) {
    content::ChildProcessData data(process_type, content::ChildProcessId());
    auto actual =
        GetMonitoredProcessInfoKeyForNonRendererChildProcessForTesting(data);
    EXPECT_EQ(actual.type, expectation.type);
    EXPECT_EQ(actual.subtype, expectation.subtype);
  }

  content::ChildProcessData utility_data(content::PROCESS_TYPE_UTILITY,
                                         content::ChildProcessId());
  utility_data.metrics_name = "TestName";
  ProcessInfo::Key expected_utility_key{MonitoredProcessType::kUtility,
                                        "TestName"};
  EXPECT_EQ(GetMonitoredProcessInfoKeyForNonRendererChildProcessForTesting(
                utility_data),
            expected_utility_key);

  // Special case for network process.
  content::ChildProcessData data(content::PROCESS_TYPE_UTILITY,
                                 content::ChildProcessId());
  data.metrics_name = network::mojom::NetworkService::Name_;
  ProcessInfo::Key expected_key{MonitoredProcessType::kNetwork, std::nullopt};
  EXPECT_EQ(
      GetMonitoredProcessInfoKeyForNonRendererChildProcessForTesting(data),
      expected_key);
}
