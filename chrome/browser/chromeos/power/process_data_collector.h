// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_PROCESS_DATA_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_POWER_PROCESS_DATA_COLLECTOR_H_

#include <sys/types.h>

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/power/process_data_collector.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace chromeos {

// A class which starts collecting metrics about processes as soon as it is
// initialized with |Initialize|. This class depends on the DBusThreadManager
// and is implemented as a global singleton.
class ProcessDataCollector {
 public:
  // The different sources of power consumption being tracked. This should be
  // kept in sync with the |PowerConsumerType| in power.js.
  enum class PowerConsumerType {
    SCREEN = 0,
    KEYBOARD = 1,
    CROSTINI = 2,
    ARC = 3,
    CHROME = 4,
    SYSTEM = 5
  };

  // Contains basic information about a process like its PID, its name, etc. The
  // |pid|, |process_name|, and |process_cmdline| will be dummy values for
  // non-process |PowerConsumerType|'s.
  struct ProcessData {
    ProcessData();
    ProcessData(pid_t pid,
                const std::string& name,
                const std::string& cmdline,
                PowerConsumerType type);
    ~ProcessData();
    ProcessData(const ProcessData& p);

    // The PID of the process.
    pid_t pid;

    // The file name of this process as invoked from the command line.
    std::string name;

    // The full command line which spawned this process.
    std::string cmdline;

    // The type of power consumer this is.
    PowerConsumerType type;
  };

  // Data that gets used for displaying process information.
  struct ProcessUsageData {
    ProcessUsageData(const ProcessData& process_data,
                     double power_usage_fraction);
    ProcessUsageData(const ProcessUsageData& p);
    ~ProcessUsageData();

    // Basic information about the process.
    ProcessData process_data;

    // The calculated amount of power usage in [0.0, 1.0]. Represents what
    // fraction of the system's total power consumption at some point was
    // consumed by this process.
    double power_usage_fraction;
  };

  // Metadata used to configure the data collector. Used to differentiate
  // between production and test environments.
  struct Config {
    // The technique used to average CPU usages to approximate battery usage.
    enum class AveragingTechnique {
      EXPONENTIAL,  // An exponential moving average.
      AVERAGE,      // A pure average.
    };

    Config(const base::FilePath& procfs,
           const base::FilePath& total_cpu_time,
           const base::FilePath& android_init,
           const std::string& cmdline_fmt,
           const std::string& stat_fmt,
           const std::string& comm_fmt,
           base::TimeDelta delay,
           AveragingTechnique technique);
    Config(const Config& s);
    ~Config();

    // Corresponds to /proc on a real machine.
    base::FilePath proc_dir;

    // Corresponds to /proc/stat on a real machine.
    base::FilePath total_cpu_time_path;

    // Corresponds to /run/containers/android-run_oci/container.pid on a real
    // machine, which contains the PID of ARC's init process.
    base::FilePath android_init_pid_path;

    // Corresponds to the general format for a process' specific cmdline file,
    // /proc/%u/cmdline on a real machine.
    std::string proc_cmdline_fmt;

    // Corresponds to the general format for a process' stat file, /proc/%u/stat
    // on a real machine.
    std::string proc_stat_fmt;

    // Corresponds to the general format for a process' comm file,
    // /proc/%u/comm on a real machine.
    std::string proc_comm_fmt;

    // The delay between sampling the procfs specified in |proc_dir|.
    base::TimeDelta sample_delay;

    // The |AveragingTechnique| that will be used to aggregate all the samples
    // for different processes collected over time.
    AveragingTechnique averaging_technique;
  };

  // Weights for an exponential moving average strategy for approximating power
  // usage.
  static constexpr double kCpuUsageExponentialMovingAverageWeight = 0.2;
  static constexpr double kPowerUsageExponentialMovingAverageWeight = 0.1;

  // Check to make sure the weights used make sense.
  static_assert(
      0 <= kCpuUsageExponentialMovingAverageWeight &&
          kCpuUsageExponentialMovingAverageWeight <= 1,
      "The weight of an exponential moving average has to be in [0., 1.]");
  static_assert(
      0 <= kPowerUsageExponentialMovingAverageWeight &&
          kPowerUsageExponentialMovingAverageWeight <= 1,
      "The weight of an exponential moving average has to be in [0., 1.]");

  // Initializes the singleton.
  static void Initialize();

  // Should be called for testing.
  static void InitializeForTesting(const Config& config);

  // Gets the global ProcessDataCollector* instance, should only be called after
  // |Initialize| or |InitializeForTesting|.
  static ProcessDataCollector* Get();

  // Should only be called after Initialize() is called and before
  // DBusThreadManager is shut down.
  static void Shutdown();

  // The analog for the |SampleCpuUsage| function but for testing. Do not call
  // this while a |ProcessDataCollector| has been initialized with |Initialize|
  // rather than |InitializeForTesting|.
  void SampleCpuUsageForTesting();

  // Gets a list of processes and their information.
  const std::vector<ProcessUsageData> GetProcessUsages();

 private:
  // A sample of a process's information at one moment of time.
  struct ProcessSample {
    ProcessSample();
    ProcessSample(const ProcessSample& p);
    ~ProcessSample();

    // Basic information about the process.
    ProcessData process_data;

    // Total CPU time consumed between the point when |ProcessDataCollector| was
    // initialized and the point at which this sample was collected.
    int64_t total_cpu_time_jiffies = 0;

    // Total CPU time consumed by this process between the point when the
    // |ProcessDataCollector| was initialized and the point at which this sample
    // was collected.
    int64_t proc_cpu_time_jiffies = 0;

    // Whether this sample represents a valid process, i.e. whether it should be
    // displayed on the chrome://power page. This is also used to indicate
    // whether a sample of a process was successfully completed, so a process
    // that was terminated while being sampled can be properly dealt with.
    // An invalid process is a process that doesn't have a displayable name,
    // has an empty cmdline, or was terminated while it was being sampled. This
    // is relevant so that ProcessStoredData knows which samples it should
    // consider when it accumulates information into its running total.
    bool valid = false;

    // The time at which this sample took place.
    base::TimeTicks now;
  };

  // Data that gets stored across samples.
  struct ProcessStoredData {
    ProcessStoredData();
    ProcessStoredData(const ProcessStoredData& p);
    ~ProcessStoredData();

    // Basic information about the process.
    ProcessData process_data;

    // The number of samples that were accumulated together at this point in
    // time.
    int64_t num_samples = 0;

    // The calculated amount of power used at this point in time; this will be
    // in [0.0, 1.0].
    double power_usage_fraction = 0;

    // The total CPU usage across |num_samples| intervals. If a1,...,an are the
    // average CPU usages across n intervals, this will aggregate those averages
    // into a single number depending on which averaging technique is specified
    // in |config_|.
    double accumulated_cpu_usages = 0;
  };

  using ProcessSampleMap = std::unordered_map<pid_t, ProcessSample>;
  using ProcessStoredDataMap = std::unordered_map<pid_t, ProcessStoredData>;

  // Only use a custom configuration while |testing|.
  explicit ProcessDataCollector(const Config& config);

  ~ProcessDataCollector();

  // Initializes |cpu_data_task_runner_| and starts |cpu_data_timer_|. This is
  // called from |Initialize| but not from |InitializeForTesting|.
  void StartSamplingCpuUsage();

  // Aggregates information, classifies, and approximates the used battery. This
  // function samples the CPU usage from the procfs specified in |config_|,
  // updates |prev_samples_| and |curr_samples_|. Then this function will
  // aggregate this information and update |curr_summary_|.
  void SampleCpuUsage();

  // Prunes all processes to only those processes that are displayable; in
  // other words, it discards all processes that have empty cmdlines or names
  // from all the processes that are read from procfs. This is a helper function
  // that is meant to be used in |ComputeSampleAsync| and returns a partially
  // filled-out map of PID's to |ProcessSample|s with the process name and the
  // process cmdline filled out. This function is static and private because it
  // needs access to |ProcessSample| and |Config|.
  static ProcessSampleMap GetValidProcesses(const Config& config);

  // Samples the CPU usage of all valid processes from |GetValidProcesses|.
  // This is a helper function meant to be used in |ComputeSampleAsync|. This
  // function returns a completely filled out set of current samples. It
  // performs the actual sampling and reads everything from procfs. Should be
  // run on the |cpu_data_task_runner_|.
  static ProcessSampleMap ComputeSample(ProcessSampleMap curr_samples,
                                        const Config& config);

  // Given a set of samples from a previous time step and a set of samples from
  // the current timestep, this function computes the aggregated information.
  static ProcessStoredDataMap ComputeSummary(
      Config::AveragingTechnique averaging_technique,
      const ProcessSampleMap& prev_samples,
      const ProcessSampleMap& curr_samples,
      const ProcessStoredDataMap& curr_summary);

  // Represents the previous and current samples as well as the summary
  // information for those set of samples.
  using SamplesAndSummaryInfo =
      std::tuple<ProcessSampleMap, ProcessSampleMap, ProcessStoredDataMap>;

  // This function is called from |SampleCpuUsage| and performs the sampling of
  // CPU usage. This function must be static; using a base::WeakPtrFactory to
  // pass in a this won't work here, since |ComputeSampleAsync| is running on
  // the |cpu_data_task_runner_|. Additionally, it must be a member because it
  // uses |Config|, which is private. This function returns the updated
  // previous and current samples after sampling from procfs.
  static SamplesAndSummaryInfo ComputeSampleAsync(
      Config config,
      ProcessSampleMap prev_samples,
      ProcessSampleMap curr_samples,
      ProcessStoredDataMap curr_summary);

  // This function saves the CPU usage information previously aggregated into
  // |curr_summary_|; this should only be run on the UI thread in order to
  // prevent data races. This function is the reply callback function of the
  // |cpu_data_task_runner_| after all the I/O operations are done. This
  // function updates the |curr_samples_| and |prev_samples_| to their new
  // values after sampling from procfs and also calculates the new battery usage
  // approximation from the current samples and the previously aggregated
  // samples.
  void SaveSamplesOnUIThread(
      const SamplesAndSummaryInfo& samples_and_summary_info);

  // The timer that's used to periodically sample CPU usage; this timer will
  // call |SampleCpuUsage| at intervals of |Config::Config::sample_delay|.
  base::RepeatingTimer cpu_data_timer_;

  // Used to sequentially run the task associated with |cpu_data_timer_| on a
  // non-UI thread. This is necessary because the sampling does IO. Also,
  // repeating tasks shouldn't be run concurrently.
  scoped_refptr<base::SequencedTaskRunner> cpu_data_task_runner_;

  // Samples before and after an interval. Used to approximate the average CPU
  // usage over this interval.
  ProcessSampleMap prev_samples_;
  ProcessSampleMap curr_samples_;

  // The summary information across all intervals samples so far for all
  // currently active processes.
  ProcessStoredDataMap curr_summary_;

  // Paths and parameters to configure the class.
  Config config_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ProcessDataCollector> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProcessDataCollector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_PROCESS_DATA_COLLECTOR_H_
