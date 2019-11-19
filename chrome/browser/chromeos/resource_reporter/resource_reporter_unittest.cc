// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <map>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/timer/mock_timer.h"
#include "base/util/memory_pressure/fake_memory_pressure_monitor.h"
#include "chrome/browser/chromeos/resource_reporter/resource_reporter.h"
#include "chrome/browser/task_manager/test_task_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using task_manager::TaskId;

namespace chromeos {

namespace {

constexpr int64_t k1KB = 1024;
constexpr int64_t k1MB = 1024 * 1024;
constexpr int64_t k1GB = 1024 * 1024 * 1024;

constexpr double kBrowserProcessCpu = 21.0;
constexpr int64_t kBrowserProcessMemory = 300 * k1MB;
constexpr double kGpuProcessCpu = 60.0;
constexpr int64_t kGpuProcessMemory = 900 * k1MB;

// A list of task records that we'll use to fill the task manager.
const ResourceReporter::TaskRecord kTestTasks[] = {
    {0, "0", 30.0, 43 * k1KB, false},
    {1, "1", 9.0, 20 * k1MB, false},
    {2, "2", 35.0, 3 * k1GB, false},
    // Browser task.
    {3, "3", kBrowserProcessCpu, kBrowserProcessMemory, false},
    {4, "4", 85.0, 400 * k1KB, false},
    {5, "5", 30.1, 500 * k1MB, false},
    // GPU task.
    {6, "6", kGpuProcessCpu, kGpuProcessMemory, false},
    {7, "7", 4.0, 1 * k1GB, false},
    {8, "8", 40.0, 64 * k1KB, false},
    {9, "9", 93.0, 64 * k1MB, false},
    {10, "10", 2.23, 2 * k1KB, false},
    {11, "11", 55.0, 40 * k1MB, false},
    {12, "12", 87.0, 30 * k1KB, false},
};

constexpr size_t kTasksSize = base::size(kTestTasks);

// A test implementation of the task manager that can be used to collect CPU and
// memory usage so that they can be tested with the resource reporter.
class DummyTaskManager : public task_manager::TestTaskManager {
 public:
  DummyTaskManager() {
    set_timer_for_testing(std::make_unique<base::MockRepeatingTimer>());
  }
  ~DummyTaskManager() override {}

  // task_manager::TestTaskManager:
  double GetPlatformIndependentCPUUsage(TaskId task_id) const override {
    // |cpu_percent| expresses the expected value that the metrics reporter
    // should give for this Task's group, which is a percentage-of-total,
    // so we need to multiply up by the number of cores, to have TaskManager
    // return the correct percentage-of-core CPU usage.
    return tasks_.at(task_id)->cpu_percent *
           base::SysInfo::NumberOfProcessors();
  }
  int64_t GetMemoryFootprintUsage(TaskId task_id) const override {
    return tasks_.at(task_id)->memory_bytes;
  }
  const std::string& GetTaskNameForRappor(TaskId task_id) const override {
    return tasks_.at(task_id)->task_name_for_rappor;
  }
  task_manager::Task::Type GetType(TaskId task_id) const override {
    switch (task_id) {
      case 3:
        return task_manager::Task::BROWSER;

      case 6:
        return task_manager::Task::GPU;

      default:
        return task_manager::Task::RENDERER;
    }
  }

  void AddTaskFromIndex(size_t index) {
    tasks_[kTestTasks[index].id] = &kTestTasks[index];
  }

  void ManualRefresh() {
    ids_.clear();
    for (const auto& pair : tasks_)
      ids_.push_back(pair.first);

    NotifyObserversOnRefreshWithBackgroundCalculations(ids_);
  }

 private:
  std::map<TaskId, const ResourceReporter::TaskRecord*> tasks_;

  DISALLOW_COPY_AND_ASSIGN(DummyTaskManager);
};

}  // namespace

class ResourceReporterTest : public testing::Test {
 public:
  ResourceReporterTest() {}
  ~ResourceReporterTest() override {}

  void SetUp() override {
    resource_reporter()->StartMonitoring(&task_manager_);
  }

  void TearDown() override { resource_reporter()->StopMonitoring(); }

  // Adds a number of tasks less than |kTopConsumersCount| to the task manager.
  void AddTasks() {
    for (size_t i = 0; i < kTasksSize; ++i)
      task_manager_.AddTaskFromIndex(i);
  }

  // Manually refresh the task manager.
  void RefreshTaskManager() {
    task_manager_.ManualRefresh();
  }

  ResourceReporter* resource_reporter() const {
    return ResourceReporter::GetInstance();
  }

  util::test::FakeMemoryPressureMonitor* monitor() { return &monitor_; }

 private:
  content::BrowserTaskEnvironment task_environment_;

  util::test::FakeMemoryPressureMonitor monitor_;

  DummyTaskManager task_manager_;

  DISALLOW_COPY_AND_ASSIGN(ResourceReporterTest);
};

// Tests that ResourceReporter::GetCpuRapporMetricName() returns the correct
// metric name that corresponds to the given CPU usage.
TEST_F(ResourceReporterTest, TestGetCpuRapporMetricName) {
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_0_TO_10_PERCENT,
            ResourceReporter::GetCpuUsageRange(0.3));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_0_TO_10_PERCENT,
            ResourceReporter::GetCpuUsageRange(5.7));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_0_TO_10_PERCENT,
            ResourceReporter::GetCpuUsageRange(9.99));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_0_TO_10_PERCENT,
            ResourceReporter::GetCpuUsageRange(10.0));

  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_10_TO_30_PERCENT,
            ResourceReporter::GetCpuUsageRange(10.1));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_10_TO_30_PERCENT,
            ResourceReporter::GetCpuUsageRange(29.99));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_10_TO_30_PERCENT,
            ResourceReporter::GetCpuUsageRange(30.0));

  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_30_TO_60_PERCENT,
            ResourceReporter::GetCpuUsageRange(30.1));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_30_TO_60_PERCENT,
            ResourceReporter::GetCpuUsageRange(59.99));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_30_TO_60_PERCENT,
            ResourceReporter::GetCpuUsageRange(60.0));

  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_ABOVE_60_PERCENT,
            ResourceReporter::GetCpuUsageRange(60.1));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_ABOVE_60_PERCENT,
            ResourceReporter::GetCpuUsageRange(100.0));
}

// Tests that ResourceReporter::GetMemoryRapporMetricName() returns the correct
// metric names for the given memory usage.
TEST_F(ResourceReporterTest, TestGetMemoryRapporMetricName) {
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_0_TO_200_MB,
            ResourceReporter::GetMemoryUsageRange(2 * k1KB));
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_0_TO_200_MB,
            ResourceReporter::GetMemoryUsageRange(20 * k1MB));
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_0_TO_200_MB,
            ResourceReporter::GetMemoryUsageRange(200 * k1MB));

  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_200_TO_400_MB,
            ResourceReporter::GetMemoryUsageRange(201 * k1MB));
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_200_TO_400_MB,
            ResourceReporter::GetMemoryUsageRange(400 * k1MB));

  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_400_TO_600_MB,
            ResourceReporter::GetMemoryUsageRange(401 * k1MB));
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_400_TO_600_MB,
            ResourceReporter::GetMemoryUsageRange(600 * k1MB));

  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_600_TO_800_MB,
            ResourceReporter::GetMemoryUsageRange(601 * k1MB));
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_600_TO_800_MB,
            ResourceReporter::GetMemoryUsageRange(800 * k1MB));

  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_800_TO_1_GB,
            ResourceReporter::GetMemoryUsageRange(801 * k1MB));
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_800_TO_1_GB,
            ResourceReporter::GetMemoryUsageRange(1 * k1GB));

  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_ABOVE_1_GB,
            ResourceReporter::GetMemoryUsageRange(1 * k1GB + 1 * k1KB));
}

// Tests all the interactions between the resource reporter and the task
// manager.
TEST_F(ResourceReporterTest, TestAll) {
  using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

  // Moderate memory pressure events should not trigger any sampling.
  monitor()->SetAndNotifyMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(resource_reporter()->observed_task_manager());

  // A critical memory pressure event, but the task manager is not tracking any
  // resource intensive tasks yet.
  monitor()->SetAndNotifyMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();
  // We should keep listening to the task manager, even after a refresh.
  RefreshTaskManager();
  EXPECT_TRUE(resource_reporter()->observed_task_manager());

  // Memory pressure reduces to moderate again, we should stop watching the task
  // manager.
  monitor()->SetAndNotifyMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(resource_reporter()->observed_task_manager());

  // Memory pressure becomes critical and we have violating tasks.
  AddTasks();
  monitor()->SetAndNotifyMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(resource_reporter()->observed_task_manager());
  RefreshTaskManager();

  // Make sure that the ResourceReporter is no longer listening to the task
  // manager right after the refresh.
  EXPECT_FALSE(resource_reporter()->observed_task_manager());

  // Make sure the ResourceReporter is not tracking any but the tasks exceeding
  // the defined resource use thresholds.
  ASSERT_FALSE(resource_reporter()->task_records_.empty());
  for (const auto& task_record : resource_reporter()->task_records_) {
    EXPECT_TRUE(task_record.cpu_percent >=
                    ResourceReporter::GetTaskCpuThresholdForReporting() ||
                task_record.memory_bytes >=
                    ResourceReporter::GetTaskMemoryThresholdForReporting());
  }

  // Make sure you have the right info about the Browser and GPU process.
  EXPECT_DOUBLE_EQ(resource_reporter()->last_browser_process_cpu_,
                   kBrowserProcessCpu);
  EXPECT_EQ(resource_reporter()->last_browser_process_memory_,
            kBrowserProcessMemory);
  EXPECT_DOUBLE_EQ(resource_reporter()->last_gpu_process_cpu_, kGpuProcessCpu);
  EXPECT_EQ(resource_reporter()->last_gpu_process_memory_, kGpuProcessMemory);
}

}  // namespace chromeos
