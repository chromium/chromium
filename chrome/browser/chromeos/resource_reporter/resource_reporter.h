// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_RESOURCE_REPORTER_RESOURCE_REPORTER_H_
#define CHROME_BROWSER_CHROMEOS_RESOURCE_REPORTER_RESOURCE_REPORTER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/singleton.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/rappor/public/sample.h"

namespace chromeos {

// A system that tracks the top |kTopConsumersCount| CPU and memory consumer
// Chrome tasks and reports a weighted random sample of them via Rappor whenever
// memory pressure is critical. The reporting is limited to once per
// |kMinimumTimeBetweenReportsInMS|.
class ResourceReporter : public task_manager::TaskManagerObserver {
 public:
  // A collection of the data of a task manager's task that the ResourceReporter
  // is interested in.
  struct TaskRecord {
    explicit TaskRecord(task_manager::TaskId task_id);

    TaskRecord(task_manager::TaskId task_id,
               const std::string& task_name,
               double cpu_percent,
               int64_t memory_bytes,
               bool background);

    // The ID of the task.
    task_manager::TaskId id;

    // The canonicalized task name to be used to represent the task in a Rappor
    // sample.
    std::string task_name_for_rappor;

    // The CPU usage of the task from the most recent task manager refresh in
    // percentage [0.0, 100.0].
    double cpu_percent;

    // The memory footprint of the task from the most recent task manager
    // refresh in bytes. It doesn't include shared memory. A value of -1 is
    // invalid and means that the memory usage measurement for this task is not
    // ready yet. See TaskManagerInterface::GetMemoryFootprintUsage().
    int64_t memory_bytes;

    // True if the task is running on a process at background priority.
    bool is_background;
  };

  ~ResourceReporter() override;

  // The singleton instance.
  static ResourceReporter* GetInstance();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Start / stop observing the task manager and the memory pressure events.
  void StartMonitoring(
      task_manager::TaskManagerInterface* task_manager_to_observe);
  void StopMonitoring();

  // task_manager::TaskManagerObserver:
  void OnTasksRefreshedWithBackgroundCalculations(
      const task_manager::TaskIdList& task_ids) override;

 private:
  friend struct base::DefaultSingletonTraits<ResourceReporter>;
  friend class ResourceReporterTest;
  FRIEND_TEST_ALL_PREFIXES(ResourceReporterTest, TestGetCpuRapporMetricName);
  FRIEND_TEST_ALL_PREFIXES(ResourceReporterTest, TestGetMemoryRapporMetricName);
  FRIEND_TEST_ALL_PREFIXES(ResourceReporterTest, TestAll);

  // WARNING: The below enum MUST never be renamed, modified or reordered, as
  // they're written to logs. You can only insert a new element immediately
  // before the last, and update the value of the last element.
  enum class TaskProcessPriority {
    FOREGROUND      = 0,
    BACKGROUND      = 1 << 0,
    NUM_PRIORITIES  = 2,
  };

  // WARNING: The below enum MUST never be renamed, modified or reordered, as
  // they're written to logs. You can only insert a new element immediately
  // before the last, and update the value of the last element.
  enum class CpuUsageRange {
    RANGE_0_TO_10_PERCENT   = 0,
    RANGE_10_TO_30_PERCENT  = 1 << 0,
    RANGE_30_TO_60_PERCENT  = 1 << 1,
    RANGE_ABOVE_60_PERCENT  = 1 << 2,
    NUM_RANGES              = 4,
  };

  // WARNING: The below enum MUST never be renamed, modified or reordered, as
  // they're written to logs. You can only insert a new element immediately
  // before the last, and update the value of the last element.
  enum class MemoryUsageRange {
    RANGE_0_TO_200_MB    = 0,
    RANGE_200_TO_400_MB  = 1 << 0,
    RANGE_400_TO_600_MB  = 1 << 1,
    RANGE_600_TO_800_MB  = 1 << 2,
    RANGE_800_TO_1_GB    = 1 << 3,
    RANGE_ABOVE_1_GB     = 1 << 4,
    NUM_RANGES           = 6,
  };

  // WARNING: The below enum MUST never be renamed, modified or reordered, as
  // they're written to logs. You can only insert a new element immediately
  // before the last, and update the value of the last element.
  enum class CpuCoresNumberRange {
    RANGE_NA              = 0,
    RANGE_1_CORE          = 1 << 0,
    RANGE_2_CORES         = 1 << 1,
    RANGE_3_TO_4_CORES    = 1 << 2,
    RANGE_5_TO_8_CORES    = 1 << 3,
    RANGE_9_TO_16_CORES   = 1 << 4,
    RANGE_ABOVE_16_CORES  = 1 << 5,
    NUM_RANGES            = 7,
  };

  ResourceReporter();

  // The CPU and memory thresholds beyond which the tasks will be reported.
  static double GetTaskCpuThresholdForReporting();
  static int64_t GetTaskMemoryThresholdForReporting();

  // Creates a Rappor sample for the given |task_record|.
  static std::unique_ptr<rappor::Sample> CreateRapporSample(
      rappor::RapporServiceImpl* rappor_service,
      const TaskRecord& task_record);

  // Gets the CPU/memory usage ranges given the |cpu| / |memory_in_bytes|
  // values.
  static CpuUsageRange GetCpuUsageRange(double cpu);
  static MemoryUsageRange GetMemoryUsageRange(int64_t memory_in_bytes);

  // Gets the bucket in which the current system's number of CPU cores fall.
  static CpuCoresNumberRange GetCurrentSystemCpuCoresRange();

  // Perform a weighted random sampling to select a task by its CPU or memory
  // usage weights so that we can record samples for them via Rappor.
  // They return nullptr if no TaskRecord has been selected.
  const TaskRecord* SampleTaskByCpu() const;
  const TaskRecord* SampleTaskByMemory() const;

  // Does the actual recording of Rappor and UMA samples.
  void ReportSamples();

  // The callback function that will be invoked on memory pressure events.
  using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;
  void OnMemoryPressure(MemoryPressureLevel memory_pressure_level);

  void StartRecordingCurrentState();
  void StopRecordingCurrentState();

  // Monitor memory pressure events.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Contains the data about the most CPU and memory intensive tasks collected
  // at a critical memory pressure event.
  std::vector<TaskRecord> task_records_;

  // Either the real task manager implementation, or a test implementation in
  // unit tests.
  task_manager::TaskManagerInterface* task_manager_to_observe_;

  // The range that includes the number of CPU cores in the current system.
  const CpuCoresNumberRange system_cpu_cores_range_;

  // The most recent reading for the browser and GPU processes to be reported as
  // UMA histograms when the system is under critical memory pressure.
  double last_browser_process_cpu_;
  double last_gpu_process_cpu_;
  int64_t last_browser_process_memory_;
  int64_t last_gpu_process_memory_;

  // Tracks whether monitoring started or not.
  bool is_monitoring_;

  DISALLOW_COPY_AND_ASSIGN(ResourceReporter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_RESOURCE_REPORTER_RESOURCE_REPORTER_H_
