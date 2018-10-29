// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/resource_reporter/resource_reporter.h"

#include <cstdint>
#include <queue>
#include <utility>

#include "base/bind.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "components/rappor/rappor_service_impl.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

#define GET_ENUM_VAL(enum_entry) static_cast<int>(enum_entry)

// At a critical memory pressure event, we only care about a single complete
// refresh from the task manager (with background calculations). So we request
// the minimum refresh rate (once per second).
constexpr int64_t kRefreshIntervalSeconds = 1;

// Various memory usage sizes in bytes.
constexpr int64_t kMemory1GB = 1024 * 1024 * 1024;
constexpr int64_t kMemory800MB = 800 * 1024 * 1024;
constexpr int64_t kMemory600MB = 600 * 1024 * 1024;
constexpr int64_t kMemory400MB = 400 * 1024 * 1024;
constexpr int64_t kMemory200MB = 200 * 1024 * 1024;

// The name of the Rappor metric to report the CPU usage.
constexpr char kCpuRapporMetric[] = "ResourceReporter.Cpu";

// The name of the Rappor metric to report the memory usage.
constexpr char kMemoryRapporMetric[] = "ResourceReporter.Memory";

// The name of the string field of the Rappor metrics in which we'll record the
// task's Rappor sample name.
constexpr char kRapporTaskStringField[] = "task";

// The name of the flags field of the Rappor metrics in which we'll store the
// priority of the process on which the task is running.
constexpr char kRapporPriorityFlagsField[] = "priority";

// The name of the flags field of the CPU usage Rappor metrics in which we'll
// record the number of cores in the current system.
constexpr char kRapporNumCoresRangeFlagsField[] = "num_cores_range";

// The name of the flags field of the Rappor metrics in which we'll store the
// CPU / memory usage ranges.
constexpr char kRapporUsageRangeFlagsField[] = "usage_range";

// Key used to store the last time a Rappor report was recorded in local_state.
constexpr char kLastRapporReportTimeKey[] =
    "resource_reporter.last_report_time";

// To keep privacy guarantees of Rappor, we limit the reports to at most once
// per day.
constexpr base::TimeDelta kMinimumTimeBetweenReports =
    base::TimeDelta::FromDays(1);

constexpr double kTaskCpuThresholdForReporting = 70.0;

}  // namespace

ResourceReporter::TaskRecord::TaskRecord(task_manager::TaskId task_id)
    : id(task_id), cpu_percent(0.0), memory_bytes(0), is_background(false) {}

ResourceReporter::TaskRecord::TaskRecord(task_manager::TaskId the_id,
                                         const std::string& task_name,
                                         double cpu_percent,
                                         int64_t memory_bytes,
                                         bool background)
    : id(the_id),
      task_name_for_rappor(task_name),
      cpu_percent(cpu_percent),
      memory_bytes(memory_bytes),
      is_background(background) {}

ResourceReporter::~ResourceReporter() {
}

// static
ResourceReporter* ResourceReporter::GetInstance() {
  return base::Singleton<ResourceReporter>::get();
}

// static
void ResourceReporter::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDoublePref(kLastRapporReportTimeKey, 0.0);
}

void ResourceReporter::StartMonitoring(
    task_manager::TaskManagerInterface* task_manager_to_observe) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_monitoring_)
    return;

  task_manager_to_observe_ = task_manager_to_observe;
  DCHECK(task_manager_to_observe_);
  is_monitoring_ = true;
  memory_pressure_listener_.reset(new base::MemoryPressureListener(
      base::Bind(&ResourceReporter::OnMemoryPressure, base::Unretained(this))));
}

void ResourceReporter::StopMonitoring() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!is_monitoring_)
    return;

  // We might be shutting down right after a critical memory pressure event, and
  // before we get an update from the task manager with all background
  // calculations refreshed. In this case we must unregister from the task
  // manager here.
  StopRecordingCurrentState();

  is_monitoring_ = false;
  memory_pressure_listener_.reset();
}

void ResourceReporter::OnTasksRefreshedWithBackgroundCalculations(
    const task_manager::TaskIdList& task_ids) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task_records_.clear();
  task_records_.reserve(task_ids.size());

  for (const auto& id : task_ids) {
    const double cpu_usage =
        (observed_task_manager()->GetPlatformIndependentCPUUsage(id) /
         base::SysInfo::NumberOfProcessors());
    const int64_t memory_usage =
        observed_task_manager()->GetMemoryFootprintUsage(id);

    // Browser and GPU processes are reported later using UMA histograms as they
    // don't have any privacy issues.
    const auto task_type = observed_task_manager()->GetType(id);
    switch (task_type) {
      case task_manager::Task::UNKNOWN:
      case task_manager::Task::ZYGOTE:
        break;

      case task_manager::Task::BROWSER:
        last_browser_process_cpu_ = cpu_usage;
        last_browser_process_memory_ = memory_usage >= 0 ? memory_usage : 0;
        break;

      case task_manager::Task::GPU:
        last_gpu_process_cpu_ = cpu_usage;
        last_gpu_process_memory_ = memory_usage >= 0 ? memory_usage : 0;
        break;

      default:
        // Other tasks types will be reported using Rappor.
        if (memory_usage < GetTaskMemoryThresholdForReporting() &&
            cpu_usage < GetTaskCpuThresholdForReporting()) {
          // We only care about CPU and memory intensive tasks.
          break;
        }

        task_records_.emplace_back(
            id, observed_task_manager()->GetTaskNameForRappor(id), cpu_usage,
            memory_usage,
            observed_task_manager()->IsTaskOnBackgroundedProcess(id));
    }
  }

  // Now that we got the data, we don't need the task manager anymore.
  if (base::MemoryPressureMonitor::Get()->GetCurrentPressureLevel() !=
          MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL ||
      !task_records_.empty()) {
    // The memory pressure events are emitted once per second. In order to avoid
    // unsubscribing and then resubscribing to the task manager again on the
    // next event, we keep listening to the task manager as long as the memory
    // pressure level is critical AND we couldn't find any violators yet.
    StopRecordingCurrentState();
  }

  // Schedule reporting the samples.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ResourceReporter::ReportSamples, base::Unretained(this)));
}

ResourceReporter::ResourceReporter()
    : TaskManagerObserver(base::TimeDelta::FromSeconds(kRefreshIntervalSeconds),
                          task_manager::REFRESH_TYPE_CPU |
                              task_manager::REFRESH_TYPE_MEMORY_FOOTPRINT |
                              task_manager::REFRESH_TYPE_PRIORITY),
      task_manager_to_observe_(nullptr),
      system_cpu_cores_range_(GetCurrentSystemCpuCoresRange()),
      last_browser_process_cpu_(0.0),
      last_gpu_process_cpu_(0.0),
      last_browser_process_memory_(0),
      last_gpu_process_memory_(0),
      is_monitoring_(false) {}

// static
double ResourceReporter::GetTaskCpuThresholdForReporting() {
  return kTaskCpuThresholdForReporting;
}

// static
int64_t ResourceReporter::GetTaskMemoryThresholdForReporting() {
  static const int64_t threshold = 0.6 *
                                   base::SysInfo::AmountOfPhysicalMemory() /
                                   base::SysInfo::NumberOfProcessors();
  return threshold;
}

// static
std::unique_ptr<rappor::Sample> ResourceReporter::CreateRapporSample(
    rappor::RapporServiceImpl* rappor_service,
    const ResourceReporter::TaskRecord& task_record) {
  std::unique_ptr<rappor::Sample> sample(
      rappor_service->CreateSample(rappor::UMA_RAPPOR_TYPE));
  sample->SetStringField(kRapporTaskStringField,
                         task_record.task_name_for_rappor);
  sample->SetFlagsField(kRapporPriorityFlagsField,
                        task_record.is_background ?
                            GET_ENUM_VAL(TaskProcessPriority::BACKGROUND) :
                            GET_ENUM_VAL(TaskProcessPriority::FOREGROUND),
                        GET_ENUM_VAL(TaskProcessPriority::NUM_PRIORITIES));
  return sample;
}

// static
ResourceReporter::CpuUsageRange
ResourceReporter::GetCpuUsageRange(double cpu) {
  if (cpu > 60.0)
    return CpuUsageRange::RANGE_ABOVE_60_PERCENT;
  if (cpu > 30.0)
    return CpuUsageRange::RANGE_30_TO_60_PERCENT;
  if (cpu > 10.0)
    return CpuUsageRange::RANGE_10_TO_30_PERCENT;

  return CpuUsageRange::RANGE_0_TO_10_PERCENT;
}

// static
ResourceReporter::MemoryUsageRange
ResourceReporter::GetMemoryUsageRange(int64_t memory_in_bytes) {
  if (memory_in_bytes > kMemory1GB)
    return MemoryUsageRange::RANGE_ABOVE_1_GB;
  if (memory_in_bytes > kMemory800MB)
    return MemoryUsageRange::RANGE_800_TO_1_GB;
  if (memory_in_bytes > kMemory600MB)
    return MemoryUsageRange::RANGE_600_TO_800_MB;
  if (memory_in_bytes > kMemory400MB)
      return MemoryUsageRange::RANGE_400_TO_600_MB;
  if (memory_in_bytes > kMemory200MB)
      return MemoryUsageRange::RANGE_200_TO_400_MB;

  return MemoryUsageRange::RANGE_0_TO_200_MB;
}

// static
ResourceReporter::CpuCoresNumberRange
ResourceReporter::GetCurrentSystemCpuCoresRange() {
  const int cpus = base::SysInfo::NumberOfProcessors();

  if (cpus > 16)
    return CpuCoresNumberRange::RANGE_ABOVE_16_CORES;
  if (cpus > 8)
    return CpuCoresNumberRange::RANGE_9_TO_16_CORES;
  if (cpus > 4)
    return CpuCoresNumberRange::RANGE_5_TO_8_CORES;
  if (cpus > 2)
    return CpuCoresNumberRange::RANGE_3_TO_4_CORES;
  if (cpus == 2)
    return CpuCoresNumberRange::RANGE_2_CORES;
  if (cpus == 1)
    return CpuCoresNumberRange::RANGE_1_CORE;

  NOTREACHED();
  return CpuCoresNumberRange::RANGE_NA;
}

const ResourceReporter::TaskRecord* ResourceReporter::SampleTaskByCpu() const {
  // Perform a weighted random sampling taking the tasks' CPU usage as their
  // weights to randomly select one of them to be reported by Rappor. The higher
  // the CPU usage, the higher the chance that the task will be selected.
  // See https://en.wikipedia.org/wiki/Reservoir_sampling.
  const TaskRecord* sampled_task = nullptr;
  double cpu_weights_sum = 0;
  for (const auto& task_data : task_records_) {
    if ((base::RandDouble() * (cpu_weights_sum + task_data.cpu_percent)) >=
        cpu_weights_sum) {
      sampled_task = &task_data;
    }
    cpu_weights_sum += task_data.cpu_percent;
  }

  return sampled_task;
}

const ResourceReporter::TaskRecord*
ResourceReporter::SampleTaskByMemory() const {
  // Perform a weighted random sampling taking the tasks' memory usage as their
  // weights to randomly select one of them to be reported by Rappor. The higher
  // the memory usage, the higher the chance that the task will be selected.
  // See https://en.wikipedia.org/wiki/Reservoir_sampling.
  const TaskRecord* sampled_task = nullptr;
  int64_t memory_weights_sum = 0;
  for (const auto& task_data : task_records_) {
    if ((base::RandDouble() * (memory_weights_sum + task_data.memory_bytes)) >=
        memory_weights_sum) {
      sampled_task = &task_data;
    }
    memory_weights_sum += task_data.memory_bytes;
  }

  return sampled_task;
}

void ResourceReporter::ReportSamples() {
  // Report browser and GPU processes usage using UMA histograms.
  UMA_HISTOGRAM_ENUMERATION(
      "ResourceReporter.BrowserProcess.CpuUsage",
      GET_ENUM_VAL(GetCpuUsageRange(last_browser_process_cpu_)),
      GET_ENUM_VAL(CpuUsageRange::NUM_RANGES));
  UMA_HISTOGRAM_ENUMERATION(
      "ResourceReporter.BrowserProcess.MemoryUsage",
      GET_ENUM_VAL(GetMemoryUsageRange(last_browser_process_memory_)),
      GET_ENUM_VAL(MemoryUsageRange::NUM_RANGES));
  UMA_HISTOGRAM_ENUMERATION(
      "ResourceReporter.GpuProcess.CpuUsage",
      GET_ENUM_VAL(GetCpuUsageRange(last_gpu_process_cpu_)),
      GET_ENUM_VAL(CpuUsageRange::NUM_RANGES));
  UMA_HISTOGRAM_ENUMERATION(
      "ResourceReporter.GpuProcess.MemoryUsage",
      GET_ENUM_VAL(GetMemoryUsageRange(last_gpu_process_memory_)),
      GET_ENUM_VAL(MemoryUsageRange::NUM_RANGES));

  // For the rest of tasks, report them using Rappor.
  auto* rappor_service = g_browser_process->rappor_service();
  if (!rappor_service || task_records_.empty())
    return;

  // We have samples to report via Rappor. Store 'now' as the time of the last
  // report.
  if (g_browser_process->local_state()) {
    g_browser_process->local_state()->SetDouble(
        kLastRapporReportTimeKey, base::Time::NowFromSystemTime().ToDoubleT());
  }

  // Use weighted random sampling to select a task to report in the CPU
  // metric.
  const TaskRecord* sampled_cpu_task = SampleTaskByCpu();
  if (sampled_cpu_task) {
    std::unique_ptr<rappor::Sample> cpu_sample(
        CreateRapporSample(rappor_service, *sampled_cpu_task));
    cpu_sample->SetFlagsField(kRapporNumCoresRangeFlagsField,
                              GET_ENUM_VAL(system_cpu_cores_range_),
                              GET_ENUM_VAL(CpuCoresNumberRange::NUM_RANGES));
    cpu_sample->SetFlagsField(
        kRapporUsageRangeFlagsField,
        GET_ENUM_VAL(GetCpuUsageRange(sampled_cpu_task->cpu_percent)),
        GET_ENUM_VAL(CpuUsageRange::NUM_RANGES));
    rappor_service->RecordSample(kCpuRapporMetric, std::move(cpu_sample));
  }

  // Use weighted random sampling to select a task to report in the memory
  // metric.
  const TaskRecord* sampled_memory_task = SampleTaskByMemory();
  if (sampled_memory_task) {
    std::unique_ptr<rappor::Sample> memory_sample(
        CreateRapporSample(rappor_service, *sampled_memory_task));
    memory_sample->SetFlagsField(
        kRapporUsageRangeFlagsField,
        GET_ENUM_VAL(GetMemoryUsageRange(sampled_memory_task->memory_bytes)),
        GET_ENUM_VAL(MemoryUsageRange::NUM_RANGES));
    rappor_service->RecordSample(kMemoryRapporMetric, std::move(memory_sample));
  }
}

void ResourceReporter::OnMemoryPressure(
    MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level ==
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    StartRecordingCurrentState();
  } else {
    StopRecordingCurrentState();
  }
}

void ResourceReporter::StartRecordingCurrentState() {
  // If we are already listening to the task manager, then we're waiting for
  // a refresh event.
  if (observed_task_manager())
    return;

  // We only record Rappor samples only if it's the first ever critical memory
  // pressure event we receive, or it has been more than
  // |kMinimumTimeBetweenReportsInMs| since the last time we recorded samples.
  if (g_browser_process->local_state()) {
    const base::Time now = base::Time::NowFromSystemTime();
    const base::Time last_rappor_report_time = base::Time::FromDoubleT(
        g_browser_process->local_state()->GetDouble(kLastRapporReportTimeKey));
    const base::TimeDelta delta_since_last_report =
        now >= last_rappor_report_time ? now - last_rappor_report_time
                                       : base::TimeDelta::Max();

    if (delta_since_last_report < kMinimumTimeBetweenReports)
      return;
  }

  // Start listening to the task manager and wait for the first refresh event
  // with background calculations completion.
  task_manager_to_observe_->AddObserver(this);
}

void ResourceReporter::StopRecordingCurrentState() {
  // If we are still listening to the task manager from an earlier critical
  // memory pressure level, we need to stop listening to it.
  if (observed_task_manager())
    observed_task_manager()->RemoveObserver(this);
}

}  // namespace chromeos
