// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/process_monitor.h"

#include <utility>

#include "base/process/process_metrics.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ProcessMonitorTest, MonitoredProcessType) {
  const std::pair<int, MonitoredProcessType> kExpectations[] = {
      {content::PROCESS_TYPE_GPU, MonitoredProcessType::kGpu},
      {content::PROCESS_TYPE_UTILITY, MonitoredProcessType::kUtility},
      {content::PROCESS_TYPE_ZYGOTE, MonitoredProcessType::kOther},
  };

  for (const auto& [process_type, expectation] : kExpectations) {
    content::ChildProcessData data(process_type, content::ChildProcessId());
    EXPECT_EQ(GetMonitoredProcessTypeForNonRendererChildProcessForTesting(data),
              expectation);
  }

  // Special case for network process.
  content::ChildProcessData data(content::PROCESS_TYPE_UTILITY,
                                 content::ChildProcessId());
  data.metrics_name = network::mojom::NetworkService::Name_;
  EXPECT_EQ(GetMonitoredProcessTypeForNonRendererChildProcessForTesting(data),
            MonitoredProcessType::kNetwork);
}

TEST(ProcessMonitorTest, UtilityProcessesRecordedTogether) {
  content::BrowserTaskEnvironment task_environment;

  ProcessMonitor pm;
  pm.AddChildProcessInfoForTesting(
      1, MonitoredProcessType::kUtility,
      base::ProcessMetrics::CreateCurrentProcessMetrics());

  class Observer : public ProcessMonitor::Observer {
   public:
    MOCK_METHOD(void,
                OnMetricsSampled,
                (MonitoredProcessType type,
                 const ProcessMonitor::Metrics& metrics),
                (override));
  };

  Observer obs;
  // OnMetricsSampled is called once per process type.
  EXPECT_CALL(obs, OnMetricsSampled(testing::Ne(MonitoredProcessType::kUtility),
                                    testing::_))
      .Times(testing::AnyNumber());

  // Exactly one call should be for the utility process type.
  EXPECT_CALL(obs, OnMetricsSampled(MonitoredProcessType::kUtility, testing::_))
      .Times(1);

  pm.SampleAllProcesses(&obs);
}
