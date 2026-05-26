// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/process_monitor.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/process/process_metrics.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/power/power_metrics_constants.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_task_environment.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoubleEq;
using ::testing::Field;
using ::testing::Ne;
using ::testing::Optional;

class FakeProcessMetricsDelegate : public ProcessMetricsDelegate {
 public:
  explicit FakeProcessMetricsDelegate(double cpu_usage)
      : cpu_usage_(cpu_usage) {}
  ~FakeProcessMetricsDelegate() override = default;

  double GetPlatformIndependentCPUUsage(
      base::TimeDelta cumulative_cpu) override {
    return cpu_usage_;
  }

  base::expected<double, base::ProcessCPUUsageError>
  GetPlatformIndependentCPUUsage() override {
    return cpu_usage_;
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_AIX)
  int GetIdleWakeupsPerSecond() override { return 0; }
#endif

#if BUILDFLAG(IS_MAC)
  int GetPackageIdleWakeupsPerSecond() override { return 0; }
#endif

 private:
  double cpu_usage_;
};

class MockObserver : public ProcessMonitor::Observer {
 public:
  MOCK_METHOD(void,
              OnMetricsSampled,
              (ProcessInfo::Key key, const ProcessMonitor::Metrics& metrics),
              (override));
  MOCK_METHOD(void,
              OnAggregatedMetricsSampled,
              (const ProcessMonitor::Metrics& metrics),
              (override));
};

}  // namespace

class ProcessMonitorIntervalTest : public ::testing::Test {
 protected:
  void FakeUtilityProcessExit(int id,
                              std::string_view metrics_name,
                              base::TimeDelta cpu_usage) {
    content::ChildProcessData data(content::PROCESS_TYPE_UTILITY,
                                   content::ChildProcessId(id));
    data.metrics_name = metrics_name;
    data.sandbox_type = sandbox::mojom::Sandbox::kService;
    content::ChildProcessTerminationInfo info;
    info.cpu_usage = cpu_usage;
    process_monitor_.BrowserChildProcessExitedNormally(data, info);
  }

  ProcessMonitor& process_monitor() { return process_monitor_; }

  content::BrowserTaskEnvironment& task_env() { return task_env_; }

 private:
  content::BrowserTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ProcessMonitor process_monitor_;
};

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
      std::make_unique<FakeProcessMetricsDelegate>(0.0));
  pm.AddChildProcessInfoForTesting(
      2, ProcessInfo::Key(MonitoredProcessType::kUtility, "Bar"),
      std::make_unique<FakeProcessMetricsDelegate>(0.0));

  MockObserver obs;

  // OnMetricsSampled is called once per process type, so check that it's called
  // once for each non-utility process.
  EXPECT_CALL(
      obs, OnMetricsSampled(Ne(ProcessInfo::Key(MonitoredProcessType::kUtility,
                                                std::nullopt)),
                            _))
      .Times(7);

  // For utility processes, it should be called once for the combination of all
  // of them, and once for each subtype.
  EXPECT_CALL(obs,
              OnMetricsSampled(
                  ProcessInfo::Key(MonitoredProcessType::kUtility, "Foo"), _))
      .Times(1);
  EXPECT_CALL(obs,
              OnMetricsSampled(
                  ProcessInfo::Key(MonitoredProcessType::kUtility, "Bar"), _))
      .Times(1);

  // Exactly one call should be for the utility process type without a subtype,
  // combining both processes of different subtypes.
  EXPECT_CALL(
      obs,
      OnMetricsSampled(
          ProcessInfo::Key(MonitoredProcessType::kUtility, std::nullopt), _))
      .Times(1);

  EXPECT_CALL(obs, OnAggregatedMetricsSampled(_)).Times(1);

  pm.SampleAllProcesses(&obs);
}

TEST_F(ProcessMonitorIntervalTest, FullInterval) {
  ProcessInfo::Key key(MonitoredProcessType::kUtility, "Foo");
  process_monitor().AddChildProcessInfoForTesting(
      1, key, std::make_unique<FakeProcessMetricsDelegate>(50.0));

  // Advance time to the end of the interval.
  task_env().FastForwardBy(kLongPowerMetricsIntervalDuration);

  // Ignore other processes.
  MockObserver obs;
  EXPECT_CALL(obs, OnMetricsSampled(Ne(key), _)).Times(AnyNumber());

  // The process averaged 50% CPU over the entire interval.
  EXPECT_CALL(obs,
              OnMetricsSampled(key, Field(&ProcessMonitor::Metrics::cpu_usage,
                                          Optional(DoubleEq(50)))));

  process_monitor().SampleAllProcesses(&obs);
}

TEST_F(ProcessMonitorIntervalTest, StartDuringInterval) {
  // Advance time to 1/4 of the interval (30 seconds).
  task_env().FastForwardBy(kLongPowerMetricsIntervalDuration / 4);

  ProcessInfo::Key key(MonitoredProcessType::kUtility, "Foo");
  process_monitor().AddChildProcessInfoForTesting(
      1, key, std::make_unique<FakeProcessMetricsDelegate>(50.0));

  // Advance time to the end of the interval (120 seconds). The process lived
  // for 90 seconds.
  task_env().FastForwardBy(kLongPowerMetricsIntervalDuration * 0.75);

  // Ignore other processes.
  MockObserver obs;
  EXPECT_CALL(obs, OnMetricsSampled(Ne(key), _)).Times(AnyNumber());

  // During its 90s lifespan in the 120s interval, the process averaged 50% CPU.
  // Correct scaled CPU usage over the 120s interval is 50.0% * 90s / 120s
  // = 37.5%.
  EXPECT_CALL(obs,
              OnMetricsSampled(key, Field(&ProcessMonitor::Metrics::cpu_usage,
                                          Optional(DoubleEq(37.5)))));

  process_monitor().SampleAllProcesses(&obs);
}

TEST_F(ProcessMonitorIntervalTest, EndDuringInterval) {
  ProcessInfo::Key key(MonitoredProcessType::kUtility, "Foo");
  process_monitor().AddChildProcessInfoForTesting(
      1, key, std::make_unique<FakeProcessMetricsDelegate>(50.0));

  // Advance time to 3/4 of the interval (90 seconds). The process lived for 90
  // seconds.
  task_env().FastForwardBy(kLongPowerMetricsIntervalDuration * 0.75);

  // The process averaged 50% CPU over 90s, or 45s of CPU.
  FakeUtilityProcessExit(1, "Foo", base::Seconds(45));

  // Advance time to the end of the interval (120 seconds).
  task_env().FastForwardBy(kLongPowerMetricsIntervalDuration * 0.25);

  // Ignore other processes.
  MockObserver obs;
  EXPECT_CALL(obs, OnMetricsSampled(Ne(key), _)).Times(AnyNumber());

  // During its 90s lifespan, the process averaged 50% CPU.
  // Correct scaled CPU usage over the 120s interval is 50.0% * 90s / 120s
  // = 37.5%.
  EXPECT_CALL(obs,
              OnMetricsSampled(key, Field(&ProcessMonitor::Metrics::cpu_usage,
                                          Optional(DoubleEq(37.5)))));

  process_monitor().SampleAllProcesses(&obs);
}

TEST_F(ProcessMonitorIntervalTest, StartAndEndDuringInterval) {
  // Advance time to 1/4 of the interval (30 seconds).
  task_env().FastForwardBy(kLongPowerMetricsIntervalDuration / 4);

  ProcessInfo::Key key(MonitoredProcessType::kUtility, "Foo");
  process_monitor().AddChildProcessInfoForTesting(
      1, key, std::make_unique<FakeProcessMetricsDelegate>(25.0));

  // Advance time to 3/4 of the interval (90 seconds). The process lived for 60
  // seconds.
  task_env().FastForwardBy(kLongPowerMetricsIntervalDuration / 2);

  // The process averaged 25% CPU over 60s, or 15s of CPU.
  FakeUtilityProcessExit(1, "Foo", base::Seconds(15));

  // Advance time to the end of the interval (120 seconds).
  task_env().FastForwardBy(kLongPowerMetricsIntervalDuration / 4);

  // Ignore other processes.
  MockObserver obs;
  EXPECT_CALL(obs, OnMetricsSampled(Ne(key), _)).Times(AnyNumber());

  // During its 60s lifespan, the process averaged 25% CPU.
  // Correct scaled CPU usage over the 120s interval is 25.0% * 60s / 120s
  // = 12.5%.
  EXPECT_CALL(obs,
              OnMetricsSampled(key, Field(&ProcessMonitor::Metrics::cpu_usage,
                                          Optional(DoubleEq(12.5)))));

  process_monitor().SampleAllProcesses(&obs);
}
