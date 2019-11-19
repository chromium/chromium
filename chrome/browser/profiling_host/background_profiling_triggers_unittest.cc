// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/background_profiling_triggers.h"

#include <set>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using GlobalMemoryDump = memory_instrumentation::GlobalMemoryDump;
using GlobalMemoryDumpPtr = memory_instrumentation::mojom::GlobalMemoryDumpPtr;
using ProcessMemoryDumpPtr =
    memory_instrumentation::mojom::ProcessMemoryDumpPtr;
using OSMemDumpPtr = memory_instrumentation::mojom::OSMemDumpPtr;
using ProcessType = memory_instrumentation::mojom::ProcessType;

namespace heap_profiling {
namespace {

constexpr uint32_t kProcessMallocTriggerKb = 2 * 1024 * 1024;  // 2 Gig

OSMemDumpPtr GetFakeOSMemDump(uint32_t resident_set_kb,
                              uint32_t private_footprint_kb,
                              uint32_t shared_footprint_kb) {
  return memory_instrumentation::mojom::OSMemDump::New(
      resident_set_kb, resident_set_kb /* peak_resident_set_kb */,
      true /* is_peak_rss_resettable */, private_footprint_kb,
      shared_footprint_kb
#if defined(OS_LINUX) || defined(OS_ANDROID)
      ,
      0
#endif
  );
}

void PopulateMetrics(GlobalMemoryDumpPtr* global_dump,
                     base::ProcessId pid,
                     ProcessType process_type,
                     uint32_t resident_set_kb,
                     uint32_t private_memory_kb,
                     uint32_t shared_footprint_kb) {
  ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->pid = pid;
  pmd->process_type = process_type;
  pmd->os_dump =
      GetFakeOSMemDump(resident_set_kb, private_memory_kb, shared_footprint_kb);
  (*global_dump)->process_dumps.push_back(std::move(pmd));
}

}  // namespace

class FakeBackgroundProfilingTriggers : public BackgroundProfilingTriggers {
 public:
  explicit FakeBackgroundProfilingTriggers(ProfilingProcessHost* host)
      : BackgroundProfilingTriggers(host),
        was_report_triggered_(false),
        should_trigger_control_report_(false) {}

  using BackgroundProfilingTriggers::OnReceivedMemoryDump;

  void Reset() {
    should_trigger_control_report_ = false;
    was_report_triggered_ = false;
    pmf_at_last_upload_.clear();
  }
  bool WasReportTriggered() const { return was_report_triggered_; }

  bool ShouldTriggerControlReport(int content_process_type) const override {
    return should_trigger_control_report_;
  }
  void SetControlTrigger(bool trigger_control_report) {
    should_trigger_control_report_ = trigger_control_report;
  }

 private:
  void TriggerMemoryReport(std::string trigger_name) override {
    was_report_triggered_ = true;
  }

  bool was_report_triggered_;
  bool should_trigger_control_report_;
};

class BackgroundProfilingTriggersTest : public testing::Test {
 public:
  BackgroundProfilingTriggersTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()),
        triggers_(&host_),
        is_metrics_enabled_(true) {}

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &is_metrics_enabled_);
    for (base::ProcessId i = 1; i <= 10; ++i)
      profiled_pids_.push_back(i);
  }

  void TearDown() override {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;

  ProfilingProcessHost host_;
  FakeBackgroundProfilingTriggers triggers_;
  std::vector<base::ProcessId> profiled_pids_;

  bool is_metrics_enabled_;
};

// Ensures:
//  * robust to empty memory dumps.
//  * does not trigger if below a size threshold.
TEST_F(BackgroundProfilingTriggersTest, OnReceivedMemoryDump_EmptyCases) {
  GlobalMemoryDumpPtr dump_empty(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  triggers_.OnReceivedMemoryDump(
      profiled_pids_, true, GlobalMemoryDump::MoveFrom(std::move(dump_empty)));
  EXPECT_FALSE(triggers_.WasReportTriggered());
  triggers_.Reset();

  GlobalMemoryDumpPtr dump_browser(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump_browser, 1, ProcessType::BROWSER, 1, 1, 1);
  triggers_.OnReceivedMemoryDump(
      profiled_pids_, true,
      GlobalMemoryDump::MoveFrom(std::move(dump_browser)));
  EXPECT_FALSE(triggers_.WasReportTriggered());
  triggers_.Reset();

  GlobalMemoryDumpPtr dump_gpu(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump_gpu, 1, ProcessType::GPU, 1, 1, 1);
  triggers_.OnReceivedMemoryDump(
      profiled_pids_, true, GlobalMemoryDump::MoveFrom(std::move(dump_gpu)));
  EXPECT_FALSE(triggers_.WasReportTriggered());
  triggers_.Reset();

  GlobalMemoryDumpPtr dump_renderer(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump_renderer, 1, ProcessType::RENDERER, 1, 1, 1);
  triggers_.OnReceivedMemoryDump(
      profiled_pids_, true,
      GlobalMemoryDump::MoveFrom(std::move(dump_renderer)));
  EXPECT_FALSE(triggers_.WasReportTriggered());
  triggers_.Reset();

  GlobalMemoryDumpPtr dump_other(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump_other, 1, ProcessType::OTHER, 1, 1, 1);
  triggers_.OnReceivedMemoryDump(
      profiled_pids_, true, GlobalMemoryDump::MoveFrom(std::move(dump_other)));
  EXPECT_FALSE(triggers_.WasReportTriggered());
  triggers_.Reset();
}

TEST_F(BackgroundProfilingTriggersTest, OnReceivedMemoryDump_ProfiledPids) {
  GlobalMemoryDumpPtr dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump, 1, ProcessType::RENDERER, 1, 1, 1);
  PopulateMetrics(&dump, 2, ProcessType::OTHER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);

  // Small processes and OTHER type processes don't trigger.
  triggers_.OnReceivedMemoryDump(profiled_pids_, true,
                                 GlobalMemoryDump::MoveFrom(std::move(dump)));
  EXPECT_FALSE(triggers_.WasReportTriggered());

  // Ensure each process type triggers.
  triggers_.Reset();
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::BROWSER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true,
                                 GlobalMemoryDump::MoveFrom(std::move(dump)));
  EXPECT_TRUE(triggers_.WasReportTriggered());

  triggers_.Reset();
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::RENDERER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true,
                                 GlobalMemoryDump::MoveFrom(std::move(dump)));
  EXPECT_TRUE(triggers_.WasReportTriggered());

  triggers_.Reset();
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::GPU, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true,
                                 GlobalMemoryDump::MoveFrom(std::move(dump)));
  EXPECT_TRUE(triggers_.WasReportTriggered());

  // Ensure control trigger work on browser process, no matter memory usage.
  triggers_.Reset();
  triggers_.SetControlTrigger(true);
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::BROWSER, 1, 1, 1);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true,
                                 GlobalMemoryDump::MoveFrom(std::move(dump)));
  EXPECT_TRUE(triggers_.WasReportTriggered());
}

TEST_F(BackgroundProfilingTriggersTest, HighWaterMark) {
  GlobalMemoryDumpPtr dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump, 1, ProcessType::BROWSER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true,
                                 GlobalMemoryDump::MoveFrom(std::move(dump)));
  EXPECT_TRUE(triggers_.WasReportTriggered());
  triggers_.Reset();

  // A small increase in memory should not trigger another report.
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  uint32_t small_increase = kProcessMallocTriggerKb + 10 * 1024;
  PopulateMetrics(&dump, 1, ProcessType::BROWSER, small_increase,
                  small_increase, small_increase);
  EXPECT_FALSE(triggers_.WasReportTriggered());

  // But a large increase should trigger another report.
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  uint32_t large_increase = kProcessMallocTriggerKb + 1000 * 1024;
  PopulateMetrics(&dump, 1, ProcessType::BROWSER, large_increase,
                  large_increase, large_increase);
  EXPECT_FALSE(triggers_.WasReportTriggered());
}

// Non-profiled processes don't trigger.
TEST_F(BackgroundProfilingTriggersTest, OnlyProfiledProcessesTrigger) {
  GlobalMemoryDumpPtr dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump, 101, ProcessType::BROWSER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true,
                                 GlobalMemoryDump::MoveFrom(std::move(dump)));
  EXPECT_FALSE(triggers_.WasReportTriggered());

  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::BROWSER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true,
                                 GlobalMemoryDump::MoveFrom(std::move(dump)));
  EXPECT_TRUE(triggers_.WasReportTriggered());
}

// Ensure IsAllowedToUpload() respects metrics collection settings.
TEST_F(BackgroundProfilingTriggersTest, IsAllowedToUpload_Metrics) {
  EXPECT_TRUE(triggers_.IsAllowedToUpload());

  is_metrics_enabled_ = false;
  EXPECT_FALSE(triggers_.IsAllowedToUpload());
}

}  // namespace heap_profiling
