// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/process_data_collector.h"

#include <sys/types.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/number_formatting.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/system/procfs_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

ProcessDataCollector* g_process_data_collector = nullptr;

namespace {

// The command line that will used to invoke the Crostini concierge daemon.
constexpr char kConciergeCmdline[] = "/usr/bin/vm_concierge";

// The full file path that will be used to invoke chrome.
constexpr char kChromeCmdPath[] = "/opt/google/chrome/chrome";

// Sampling frequency.
constexpr base::TimeDelta kSampleDelay = base::TimeDelta::FromSeconds(15);

// Time after which a sample is invalid. Must be greater than |kSampleDelay|.
constexpr base::TimeDelta kExcessiveDelay = base::TimeDelta::FromSeconds(30);

// Represents a map of all processes; maps a PPID to a PID.
using PpidToPidMap = std::unordered_multimap<pid_t, pid_t>;

// Represents the CPU and power exponential moving averages.
struct CpuUsageAndPowerAverage {
  // Represents an accumulated average; this will change depending on the
  // |AveragingTechnique| specified.
  double accumulated_cpu_usages;

  // Represents the the average power usage that is calculated from the
  // |accumulated_cpu_usages|. How it is calculated depends on the
  // |AveragingTechnique|.
  double power_average;
};

// Represents the CPU time a process has used and its PPID.
using ProcCpuUsageAndPpid = std::pair<int64_t, pid_t>;

// Returns ARC's init PID.
base::Optional<pid_t> GetAndroidInitPid(
    const base::FilePath& android_pid_file) {
  // This function does I/O and must be done on a blocking thread.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string android_pid_contents;
  if (!base::ReadFileToString(android_pid_file, &android_pid_contents))
    return base::nullopt;

  // This file contains a single number which contains the PID of the Android
  // init PID.
  pid_t android_pid;
  base::TrimWhitespaceASCII(android_pid_contents, base::TRIM_ALL,
                            &android_pid_contents);
  if (!base::StringToInt(android_pid_contents, &android_pid))
    return base::nullopt;

  return android_pid;
}

// Calculates the total CPU time used by a single process in jiffies and its
// PPID.
base::Optional<ProcCpuUsageAndPpid> ComputeProcCpuTimeJiffiesAndPpid(
    const base::FilePath& proc_stat_file) {
  // This function does I/O and must be done on a blocking thread.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::Optional<system::SingleProcStat> stat =
      system::GetSingleProcStat(proc_stat_file);
  if (!stat.has_value())
    return base::nullopt;

  return std::make_pair(stat.value().utime + stat.value().stime,
                        stat.value().ppid);
}

// Reads a process' name from |comm_file|, a file like "/proc/%u/comm".
base::Optional<std::string> GetProcName(const base::FilePath& comm_file) {
  // This function does I/O and must be done on a blocking thread.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string comm_contents;
  if (!base::ReadFileToString(comm_file, &comm_contents))
    return base::nullopt;

  base::TrimWhitespaceASCII(comm_contents, base::TRIM_ALL, &comm_contents);

  return comm_contents.empty() ? base::nullopt
                               : base::make_optional(comm_contents);
}

// Reads a process's command line from |cmdline|, a path like
// "/proc/%u/cmdline".
base::Optional<std::string> GetProcCmdline(const base::FilePath& cmdline) {
  // This function does I/O and must be done on a blocking thread.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string cmdline_contents;
  if (!base::ReadFileToString(cmdline, &cmdline_contents))
    return base::nullopt;

  base::TrimWhitespaceASCII(cmdline_contents, base::TRIM_ALL,
                            &cmdline_contents);

  return cmdline_contents.empty() ? base::nullopt
                                  : base::make_optional(cmdline_contents);
}

// Finds all children of a root PID recursively and stores the results in
// |visited|.
void GetChildPids(const PpidToPidMap& ppid_to_pid,
                  pid_t root,
                  std::unordered_set<pid_t>* visited) {
  visited->insert(root);
  auto adj = ppid_to_pid.equal_range(root);
  for (auto iter = adj.first; iter != adj.second; iter++) {
    pid_t pid = iter->second;
    if (visited->find(pid) == visited->end())
      GetChildPids(ppid_to_pid, pid, visited);
  }
}

// Returns a new average given a |summed_averages| aggregated over |num_samples|
// samples and a |new_sample|.
CpuUsageAndPowerAverage ComputeNormalAverages(int64_t num_samples,
                                              double summed_averages,
                                              double new_sample) {
  // When the |AveragingTechnique| used is |AveragingTechnique::AVERAGE|, the
  // |summed_averages| that should be passed into this function should be
  // accumulated sums of the average CPU usages over all of the previous
  // intervals. Specifically, say that n averages over n intervals have been
  // calculated, a1,...,an and the a(n+1)th sample is just passed, then:
  // |num_samples| = n
  // |summed_averages| = a1 + a2 + ... + an
  // |new_sample| = a(n+1)
  double accumulated_cpu_usages = summed_averages + new_sample;
  double new_power_average = accumulated_cpu_usages / (num_samples + 1);
  return {accumulated_cpu_usages, new_power_average};
}

// Returns new exponential moving averages calculated as follows:
// Let a = the weight for CPU averages and b = the weight for power averages.
// First calculate:
// c_new = (1 - a) * c_old + a * c_curr
// b_new = (1 - b) * b_old + b * c_new
// Where c_new is the new exponential moving average for CPU usages, c_old is
// the currently aggregated CPU usages, and c_curr is the new CPU average over a
// single interval. Similarly, b_old is the old power exponential moving
// average and b_new is the newly calculated exponential moving average. The
// function returns c_new and b_new.
CpuUsageAndPowerAverage ComputeExponentialMovingAverages(
    double curr_cpu_average,
    double curr_power_average,
    double new_sample) {
  double new_cpu_average =
      (1 - ProcessDataCollector::kCpuUsageExponentialMovingAverageWeight) *
          curr_cpu_average +
      ProcessDataCollector::kCpuUsageExponentialMovingAverageWeight *
          new_sample;
  double new_power_average =
      (1 - ProcessDataCollector::kPowerUsageExponentialMovingAverageWeight) *
          curr_power_average +
      ProcessDataCollector::kPowerUsageExponentialMovingAverageWeight *
          new_cpu_average;
  return {new_cpu_average, new_power_average};
}

}  // namespace

ProcessDataCollector::ProcessUsageData::ProcessUsageData(
    const ProcessData& process_data,
    double power_usage_fraction)
    : process_data(process_data), power_usage_fraction(power_usage_fraction) {}

ProcessDataCollector::ProcessUsageData::ProcessUsageData(
    const ProcessUsageData& p) = default;

ProcessDataCollector::ProcessUsageData::~ProcessUsageData() = default;

ProcessDataCollector::Config::Config(const base::FilePath& procfs,
                                     const base::FilePath& total_cpu_time,
                                     const base::FilePath& android_init,
                                     const std::string& cmdline_fmt,
                                     const std::string& stat_fmt,
                                     const std::string& comm_fmt,
                                     base::TimeDelta delay,
                                     AveragingTechnique technique)
    : proc_dir(procfs),
      total_cpu_time_path(total_cpu_time),
      android_init_pid_path(android_init),
      proc_cmdline_fmt(cmdline_fmt),
      proc_stat_fmt(stat_fmt),
      proc_comm_fmt(comm_fmt),
      sample_delay(delay),
      averaging_technique(technique) {}

ProcessDataCollector::Config::Config(const Config& config) = default;

ProcessDataCollector::Config::~Config() = default;

// static
void ProcessDataCollector::Initialize() {
  DCHECK(!g_process_data_collector);

  // A |ProcessDataCollector::Config| struct that contains a set of parameters
  // to be used in a production environment.
  const ProcessDataCollector::Config kRealConfig(
      // /proc directory which stores process information.
      base::FilePath("/proc"),
      // Contains the total amount of CPU time used.
      base::FilePath("/proc/stat"),
      // Contains the PID of the ARC init process.
      base::FilePath("/run/containers/android-run_oci/container.pid"),
      // Contains the command used to start a process.
      "/proc/%u/cmdline",
      // Contains the amount of CPU time a process has used and its PPID.
      "/proc/%u/stat",
      // Contains the name of the process.
      "/proc/%u/comm", kSampleDelay,
      ProcessDataCollector::Config::AveragingTechnique::AVERAGE);

  g_process_data_collector = new ProcessDataCollector(kRealConfig);
  g_process_data_collector->StartSamplingCpuUsage();
}

// static
void ProcessDataCollector::InitializeForTesting(const Config& config) {
  DCHECK(!g_process_data_collector);
  g_process_data_collector = new ProcessDataCollector(config);
}

// static
ProcessDataCollector* ProcessDataCollector::Get() {
  DCHECK(g_process_data_collector);
  return g_process_data_collector;
}

// static
void ProcessDataCollector::Shutdown() {
  DCHECK(g_process_data_collector);
  delete g_process_data_collector;
  g_process_data_collector = nullptr;
}

void ProcessDataCollector::SampleCpuUsageForTesting() {
  DCHECK(g_process_data_collector);
  SamplesAndSummaryInfo samples_and_summary_info =
      g_process_data_collector->ComputeSampleAsync(
          g_process_data_collector->config_,
          g_process_data_collector->prev_samples_,
          g_process_data_collector->curr_samples_,
          g_process_data_collector->curr_summary_);
  g_process_data_collector->SaveSamplesOnUIThread(samples_and_summary_info);
}

const std::vector<ProcessDataCollector::ProcessUsageData>
ProcessDataCollector::GetProcessUsages() {
  std::vector<ProcessUsageData> process_list;

  // Gather the summarized statistics and generate user-facing information.
  for (const auto& proc : curr_summary_) {
    process_list.emplace_back(ProcessDataCollector::ProcessUsageData(
        ProcessData(proc.second.process_data),
        proc.second.power_usage_fraction));
  }

  // Since the sampling will give us approximate CPU usages and some processes
  // are discarded in |GetValidProcesses|, the total fraction of CPU usages in
  // this list of processes may not sum up to 1. Normalizing ensures that the
  // fractions make sense.
  double total = std::accumulate(
      process_list.begin(), process_list.end(), 0.,
      [](const auto& i, const auto& s) { return i + s.power_usage_fraction; });
  if (total != 0) {
    std::for_each(process_list.begin(), process_list.end(),
                  [&total](auto& c) { c.power_usage_fraction /= total; });
  }

  return process_list;
}

ProcessDataCollector::ProcessData::ProcessData() = default;
ProcessDataCollector::ProcessData::ProcessData(pid_t pid,
                                               const std::string& name,
                                               const std::string& cmdline,
                                               PowerConsumerType type)
    : pid(pid), name(name), cmdline(cmdline), type(type) {}
ProcessDataCollector::ProcessData::ProcessData(const ProcessData& p) = default;
ProcessDataCollector::ProcessData::~ProcessData() = default;

ProcessDataCollector::ProcessSample::ProcessSample() = default;
ProcessDataCollector::ProcessSample::ProcessSample(const ProcessSample& p) =
    default;
ProcessDataCollector::ProcessSample::~ProcessSample() = default;

ProcessDataCollector::ProcessStoredData::ProcessStoredData() = default;
ProcessDataCollector::ProcessStoredData::ProcessStoredData(
    const ProcessStoredData& p) = default;
ProcessDataCollector::ProcessStoredData::~ProcessStoredData() = default;

ProcessDataCollector::ProcessDataCollector(const Config& config)
    : config_(config) {}

ProcessDataCollector::~ProcessDataCollector() = default;

void ProcessDataCollector::StartSamplingCpuUsage() {
  cpu_data_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  cpu_data_timer_.Start(FROM_HERE, config_.sample_delay, this,
                        &ProcessDataCollector::SampleCpuUsage);
}

void ProcessDataCollector::SampleCpuUsage() {
  base::PostTaskAndReplyWithResult(
      cpu_data_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ProcessDataCollector::ComputeSampleAsync, config_,
                     prev_samples_, curr_samples_, curr_summary_),
      base::BindOnce(&ProcessDataCollector::SaveSamplesOnUIThread,
                     weak_ptr_factory_.GetWeakPtr()));
}

// static
ProcessDataCollector::ProcessSampleMap ProcessDataCollector::GetValidProcesses(
    const Config& config) {
  // This function does I/O and must be done on a blocking thread.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::FileEnumerator proc_files(config.proc_dir, false,
                                  base::FileEnumerator::DIRECTORIES);
  ProcessSampleMap procs;

  for (base::FilePath path = proc_files.Next(); !path.empty();
       path = proc_files.Next()) {
    pid_t proc;
    if (!base::StringToInt(path.BaseName().value(), &proc))
      continue;

    // Don't track if either the process name or cmdline are empty or
    // non-existent.
    base::Optional<std::string> proc_name = GetProcName(
        base::FilePath(base::StringPrintf(config.proc_comm_fmt.c_str(), proc)));
    if (!proc_name)
      continue;
    base::Optional<std::string> proc_cmdline = GetProcCmdline(base::FilePath(
        base::StringPrintf(config.proc_cmdline_fmt.c_str(), proc)));
    if (!proc_cmdline)
      continue;

    ProcessSample psample;
    psample.valid = true;
    psample.process_data.pid = proc;
    psample.process_data.name = proc_name.value();
    psample.process_data.cmdline = proc_cmdline.value();
    // Set every process type to be a system process. Once procfs is sampled in
    // more detail, the actual type of power consumer can be set and determined.
    psample.process_data.type = ProcessDataCollector::PowerConsumerType::SYSTEM;
    psample.now = base::TimeTicks::Now();

    // |procs| starts off as an empty |ProcessSampleMap| and for every
    // iteration of this loop, |proc| will correspond to the PID of a different
    // process. Thus, this call to |std::unordered_map::emplace| should never be
    // called with the same key twice.
    procs.emplace(proc, std::move(psample));
  }

  return procs;
}

// static
ProcessDataCollector::ProcessSampleMap ProcessDataCollector::ComputeSample(
    ProcessSampleMap curr_samples,
    const Config& config) {
  // First the amount of CPU time used by the machine is gathered. Then, for
  // each process, the PPID and the amount of CPU time it has used is gathered.
  // All this information is stored in the process sample so that an average can
  // later be calculated. Additionally, a PPID to PID map is constructed so that
  // different types of processes can be classified; this is needed to classify
  // ARC process for example.
  base::Optional<int64_t> total_cpu_time =
      system::GetCpuTimeJiffies(config.total_cpu_time_path);
  // If this can't be read, then the average CPU usage over this interval can't
  // be calculated. Ignore these samples.
  if (!total_cpu_time.value()) {
    // Set all the samples to be invalid.
    for (auto& sample : curr_samples)
      sample.second.valid = false;
    return curr_samples;
  }

  base::Optional<int64_t> concierge_pid = base::nullopt;
  std::unordered_map<pid_t, int64_t> pid_to_cpu_usage_before;
  std::unordered_set<uint64_t> chrome_pids;
  PpidToPidMap proc_ppid_to_pid;

  for (auto& sample : curr_samples) {
    base::Optional<ProcCpuUsageAndPpid> proc_cpu_time_and_ppid =
        ComputeProcCpuTimeJiffiesAndPpid(base::FilePath(
            base::StringPrintf(config.proc_stat_fmt.c_str(), sample.first)));

    // If this failed, it could be that a process terminated while sampling it.
    // Ignore these processes; since the process will be invalid, it will never
    // be aggregated into |ProcessStoredData|.
    sample.second.valid = proc_cpu_time_and_ppid.has_value();
    if (!sample.second.valid)
      continue;

    uint64_t proc_cpu_time = proc_cpu_time_and_ppid.value().first;
    pid_t ppid = proc_cpu_time_and_ppid.value().second;

    sample.second.total_cpu_time_jiffies = total_cpu_time.value();
    sample.second.proc_cpu_time_jiffies = proc_cpu_time;
    proc_ppid_to_pid.insert({ppid, sample.first});

    // Get the PID of the Concierge daemon if it exists.
    if (base::StartsWith(sample.second.process_data.cmdline, kConciergeCmdline,
                         base::CompareCase::SENSITIVE))
      concierge_pid = sample.second.process_data.pid;

    // Get all process that are Chrome processes.
    if (base::StartsWith(sample.second.process_data.cmdline, kChromeCmdPath,
                         base::CompareCase::SENSITIVE))
      chrome_pids.insert(sample.first);
  }

  std::unordered_set<pid_t> arc_pids;
  base::Optional<pid_t> android_init_pid =
      GetAndroidInitPid(config.android_init_pid_path);
  // Compute all processes associated with ARC.
  if (android_init_pid.has_value())
    GetChildPids(proc_ppid_to_pid, android_init_pid.value(), &arc_pids);

  std::unordered_set<pid_t> crostini_pids;
  // Compute all process associated with Crostini.
  if (concierge_pid.has_value())
    GetChildPids(proc_ppid_to_pid, concierge_pid.value(), &crostini_pids);

  // Change the process types appropriately depending on the previous
  // information gathered. Currently, these are the only 3 types of process that
  // are classified. Everything else is classified as a |SYSTEM| process, which
  // was the default value set in |GetValidProcesses|.
  for (auto& sample : curr_samples) {
    if (arc_pids.count(sample.first)) {
      sample.second.process_data.type =
          ProcessDataCollector::PowerConsumerType::ARC;
    } else if (crostini_pids.count(sample.first)) {
      sample.second.process_data.type =
          ProcessDataCollector::PowerConsumerType::CROSTINI;
    } else if (chrome_pids.count(sample.first)) {
      sample.second.process_data.type =
          ProcessDataCollector::PowerConsumerType::CHROME;
    }
  }

  return curr_samples;
}

// static
ProcessDataCollector::ProcessStoredDataMap ProcessDataCollector::ComputeSummary(
    Config::AveragingTechnique averaging_technique,
    const ProcessSampleMap& prev_samples,
    const ProcessSampleMap& curr_samples,
    const ProcessStoredDataMap& curr_summary) {
  ProcessStoredDataMap next_summary;

  // For each pair of valid samples, samples that are neither invalid nor
  // separated by a time delay greater than a preset amount, across two
  // timeslices, calculate the average CPU usage for each process. Once this has
  // been calculated, aggregate this average of one interval to the average
  // across all intervals this process has been sampled for; the average can be
  // a normal average or an exponential moving average. Store the newly updated
  // averages into ProcessStoredData and update |curr_summary_| with the latest
  // calculations.
  for (const auto& sample : curr_samples) {
    auto prev_iter = prev_samples.find(sample.first);
    // If the process has not been sampled for more than two timeslices, then
    // there is no way to compute an average across an interval, so skip this
    // process for now. When the next timeslice comes around, if the process is
    // still running, it can be sampled again and there will be two samples in
    // two different timeslices so that an average over an interval can be
    // computed.
    if (prev_iter == prev_samples.end())
      continue;
    const ProcessSample& curr_sample = sample.second;
    const ProcessSample& prev_sample = prev_iter->second;

    // If there's too much of a delay between the last sample and our current
    // sample, then throw away all of the previous information, since there was
    // some problem that caused it to delay.
    if (base::TimeDelta(curr_sample.now - prev_sample.now) > kExcessiveDelay)
      continue;

    // Computes the average CPU usage over a single interval.
    double cpu_usage =
        (static_cast<double>(curr_sample.proc_cpu_time_jiffies -
                             prev_sample.proc_cpu_time_jiffies)) /
        (static_cast<double>(curr_sample.total_cpu_time_jiffies -
                             prev_sample.total_cpu_time_jiffies));

    ProcessStoredData& new_summary = next_summary[sample.first];

    new_summary.process_data.pid = curr_sample.process_data.pid;
    new_summary.process_data.name = curr_sample.process_data.name;
    new_summary.process_data.type = curr_sample.process_data.type;
    new_summary.process_data.cmdline = curr_sample.process_data.cmdline;

    auto summary_iter = curr_summary.find(sample.first);
    // If this is the first sample, there is no power usage and only one
    // sample.
    if (summary_iter == curr_summary.end()) {
      new_summary.accumulated_cpu_usages = cpu_usage;
      new_summary.power_usage_fraction = cpu_usage;
      new_summary.num_samples = 1;
    } else {
      // Otherwise, depending on the averaging technique calculate the average
      // CPU usage across all processes so far; e.g. for time step i + 1,
      // ProcessStoredData should have aggregated the first i time steps.
      if (averaging_technique ==
          ProcessDataCollector::Config::AveragingTechnique ::EXPONENTIAL) {
        const CpuUsageAndPowerAverage averages =
            ComputeExponentialMovingAverages(
                summary_iter->second.accumulated_cpu_usages,
                summary_iter->second.power_usage_fraction, cpu_usage);
        new_summary.accumulated_cpu_usages = averages.accumulated_cpu_usages;
        new_summary.power_usage_fraction = averages.power_average;
      } else if (averaging_technique ==
                 ProcessDataCollector::Config::AveragingTechnique ::AVERAGE) {
        const CpuUsageAndPowerAverage averages = ComputeNormalAverages(
            summary_iter->second.num_samples,
            summary_iter->second.accumulated_cpu_usages, cpu_usage);
        new_summary.accumulated_cpu_usages = averages.accumulated_cpu_usages;
        new_summary.power_usage_fraction = averages.power_average;
      } else {
        NOTREACHED();
      }
      new_summary.num_samples = summary_iter->second.num_samples + 1;
    }
  }

  return next_summary;
}

// static
ProcessDataCollector::SamplesAndSummaryInfo
ProcessDataCollector::ComputeSampleAsync(Config config,
                                         ProcessSampleMap prev_samples,
                                         ProcessSampleMap curr_samples,
                                         ProcessStoredDataMap curr_summary) {
  ProcessSampleMap procs = GetValidProcesses(config);

  prev_samples = std::move(curr_samples);
  curr_samples = ComputeSample(std::move(procs), config);
  ProcessStoredDataMap next_summary = ComputeSummary(
      config.averaging_technique, prev_samples, curr_samples, curr_summary);

  return std::make_tuple(std::move(prev_samples), std::move(curr_samples),
                         std::move(next_summary));
}

void ProcessDataCollector::SaveSamplesOnUIThread(
    const SamplesAndSummaryInfo& samples_and_summary_info) {
  // Modifies |prev_samples_|, |curr_samples_|, and |next_summary|, thus this
  // must be run on the UI thread.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::tie(prev_samples_, curr_samples_, curr_summary_) =
      samples_and_summary_info;
}

}  // namespace chromeos
