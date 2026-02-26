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

TEST(ProcessMonitorTest, UtilityProcessesRecordedTogether) {
  content::BrowserTaskEnvironment task_environment;

  ProcessMonitor pm;
  pm.AddChildProcessInfoForTesting(
      1, ProcessInfo::Key(MonitoredProcessType::kUtility, "Foo"),
      base::ProcessMetrics::CreateCurrentProcessMetrics());
  pm.AddChildProcessInfoForTesting(
      2, ProcessInfo::Key(MonitoredProcessType::kUtility, "Bar"),
      base::ProcessMetrics::CreateCurrentProcessMetrics());

  class Observer : public ProcessMonitor::Observer {
   public:
    MOCK_METHOD(void,
                OnMetricsSampled,
                (ProcessInfo::Key key, const ProcessMonitor::Metrics& metrics),
                (override));
  };

  Observer obs;
  // OnMetricsSampled is called once per process type, so check that it's called
  // once for each non-utility process.
  EXPECT_CALL(
      obs, OnMetricsSampled(testing::Ne(ProcessInfo::Key(
                                MonitoredProcessType::kUtility, std::nullopt)),
                            testing::_))
      .Times(7);

  // For utility processes, it should be called once for the combination of all
  // of them, and once for each subtype.
  EXPECT_CALL(obs, OnMetricsSampled(
                       ProcessInfo::Key(MonitoredProcessType::kUtility, "Foo"),
                       testing::_))
      .Times(1);
  EXPECT_CALL(obs, OnMetricsSampled(
                       ProcessInfo::Key(MonitoredProcessType::kUtility, "Bar"),
                       testing::_))
      .Times(1);

  // Exactly one call should be for the utility process type without a subtype,
  // combining both processes of different subtypes.
  EXPECT_CALL(
      obs, OnMetricsSampled(
               ProcessInfo::Key(MonitoredProcessType::kUtility, std::nullopt),
               testing::_))
      .Times(1);

  pm.SampleAllProcesses(&obs);
}
