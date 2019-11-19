// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>

#include <array>
#include <cstring>
#include <fstream>
#include <memory>
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/power/process_data_collector.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

// The tests are structured as follows; this is taken from |RunTest|:
// 1. First, there's a call to |SetUpProcfs| which sets up the dummy procfs that
//    will be used during testing.
// 2. |TimeStepExpectedResult| structures are constructed, which contain the
//    expected results for each time step that the |ProcessDataCollector| is
//    sampled.
// 3. |RunTest| is called which calls |ValidateProcessList| to check whether the
//    |TimeStepExpectedResult|s for each time step match; after a time step is
//    checked, the procfs of the next time step is set up with |SetUpProcfs|.

namespace chromeos {

namespace {

// The path component for a custom stat; this corresponds to a real /proc/stat.
constexpr char kCustomCpuTimeFile[] = "stat";

// The path component for a custom file where Android's init PID lives.
constexpr char kCustomAndroidInitPidFile[] = "container.pid";

// The number of jiffies per iteration of the simulated procfs.
constexpr uint64_t kJiffiesPerIteration = 100;

using ProcessData = ProcessDataCollector::ProcessData;
using Config = ProcessDataCollector::Config;
using PowerConsumerType = ProcessDataCollector::PowerConsumerType;
using ProcessUsageData = ProcessDataCollector::ProcessUsageData;

// A process information container containing the minimum amount of information
// for testing.
struct ProcessTestData {
  ProcessTestData() = default;
  ProcessTestData(ProcessData process_data,
                  pid_t ppid,
                  double cpu_usage_fraction)
      : process_data(process_data),
        ppid(ppid),
        cpu_usage_fraction(cpu_usage_fraction) {}

  // Information about the process.
  ProcessData process_data;

  // The PPID of the process.
  pid_t ppid = 0;

  // The fraction of CPU used by this process.
  double cpu_usage_fraction = 0.;
};

// The total number of entries that will be in the simulated /proc/%u/stat.
constexpr uint64_t kProcStatNumEntries = 52;

// The number of faked data entries in the following data. Update this number as
// necessary.
constexpr uint64_t kNumProcStatFakedEntries = 16;

// The number of zeroed entries.
constexpr uint64_t kNumProcStatZeroedEntries =
    kProcStatNumEntries - kNumProcStatFakedEntries;

// Generates a suitable string for to be written in a /proc/%u/stat.
std::string GenerateProcPidStatString(const std::string& name,
                                      pid_t ppid,
                                      uint64_t user_time,
                                      uint64_t kernel_time) {
  return base::StringPrintf(
             "0 (%s) S %" PRId32 " %s %" PRIu64 " %" PRIu64 " ", name.c_str(),
             ppid,
             // Pad 9 0's between the PPID and the user and
             // kernel times.
             base::JoinString(std::vector<std::string>(9, "0"), " ").c_str(),
             user_time, kernel_time) +
         base::JoinString(
             std::vector<std::string>(kNumProcStatZeroedEntries, "0"), " ") +
         "\n";
}

// Writes |data| to |file_path| and checks that the write succeeded.
void WriteStringToFile(const base::FilePath& file_path,
                       const std::string& data) {
  ASSERT_EQ(base::WriteFile(file_path, data.c_str(), data.length()),
            static_cast<int>(data.length()));
}

// Sets up a procfs-like directory with certain processes and stat files
// corresponding to a certain time step within a custom directory. For example,
// say a process has a fixed CPU usage of 0.03 or 4% of the total CPU usage.
// Then the number of jiffies used would be this;
// timestep: 3
// timestep: 6
// timestep: 9
// ...
// When written /proc/%u/stat, this total will be split into the user and kernel
// times. Additionally, each time step, the CPU itself uses 100 jiffies. So for
// the first time step this process will 3 / 100 = 0.03 of the total CPU, and
// the second time step, (3 + 3) / (100 + 100) = 6 / 200 = 0.03. Thus the
// fraction of CPU usage will remain fixed.
void SetUpProcfs(const base::FilePath& root_dir,
                 const std::vector<ProcessTestData>& processes,
                 int iteration) {
  for (const auto& process_info : processes) {
    uint64_t jiffies = (iteration + 1) * kJiffiesPerIteration *
                       process_info.cpu_usage_fraction;
    std::string proc_stat_file_contents = GenerateProcPidStatString(
        process_info.process_data.name, process_info.ppid,
        // Above, the number of jiffies used in total by the process was
        // calculated for this time step. If the number of jiffies is odd,
        // division by 2 won't work since it'll be one short, so compensate for
        // that.
        jiffies / 2 /* user time */, (jiffies + 1) / 2 /* kernel time*/);
    std::string proc_comm_file_contents = process_info.process_data.name + "\n";

    ASSERT_TRUE(base::CreateDirectory(base::FilePath(base::StringPrintf(
        "%s/%u", root_dir.value().c_str(), process_info.process_data.pid))));
    WriteStringToFile(base::FilePath(base::StringPrintf(
                          // Corresponds to a real /proc/%u/cmdline.
                          "%s/%u/cmdline", root_dir.value().c_str(),
                          process_info.process_data.pid)),
                      process_info.process_data.cmdline);
    WriteStringToFile(base::FilePath(base::StringPrintf(
                          // Correponds to a real /proc/%u/stat.
                          "%s/%u/stat", root_dir.value().c_str(),
                          process_info.process_data.pid)),
                      proc_stat_file_contents);
    WriteStringToFile(base::FilePath(base::StringPrintf(
                          // Corresponds to a real /proc/%u/comm.
                          "%s/%u/comm", root_dir.value().c_str(),
                          process_info.process_data.pid)),
                      proc_comm_file_contents);
  }

  // Let each time step use |kJiffiesPerIteration| jiffies.
  std::string stat_file_contents =
      "cpu " + std::to_string(kJiffiesPerIteration * (iteration + 1)) +
      // Pad some zeros.
      " 0 0 0 0 0 0 0 0 0\n";

  WriteStringToFile(base::FilePath(base::StringPrintf(
                        "%s/%s", root_dir.value().c_str(), kCustomCpuTimeFile)),
                    stat_file_contents);

  // The hard coded PID used for Android's init process.
  std::string android_init_pid_contents = "10\n";
  WriteStringToFile(
      base::FilePath(base::StringPrintf("%s/%s", root_dir.value().c_str(),
                                        kCustomAndroidInitPidFile)),
      android_init_pid_contents);
}

// Represents the expected results for a single time step.
struct TimeStepExpectedResult {
  TimeStepExpectedResult() = default;
  TimeStepExpectedResult(
      const std::unordered_set<pid_t>& expected_pids,
      const std::unordered_map<pid_t, double>& expected_avgs,
      const std::unordered_map<pid_t, ProcessTestData>& pid_to_proc)
      : expected_pids(expected_pids),
        expected_averages(expected_avgs),
        pid_infos(pid_to_proc) {}

  // The expected PID's to be returned; these should only be valid PIDs.
  std::unordered_set<pid_t> expected_pids;

  // The expected averages.
  std::unordered_map<pid_t, double> expected_averages;

  // Maps PID's to |ProcessTestData| structs. This is used to valid process
  // types and process names.
  std::unordered_map<pid_t, ProcessTestData> pid_infos;
};

// Normalizes the average CPU usages of processes with |valid_pids| such
// that their sum is 1.0.
void NormalizeProcessAverages(
    const std::unordered_set<pid_t>& valid_pids,
    std::unordered_map<pid_t, double>* process_averages) {
  double total = 0.;
  for (const auto& pid : valid_pids)
    total += (*process_averages)[pid];
  if (total == 0.)
    return;  // No more work needs to be done.
  for (auto& average : *process_averages)
    average.second /= total;
}

}  // namespace

class ProcessDataCollectorTest : public testing::Test {
 public:
  ProcessDataCollectorTest() {
    InitializeStats();

    // Create a temporary directory for the simulated procfs.
    CHECK(proc_dir_.CreateUniqueTempDir());
  }

  ~ProcessDataCollectorTest() override { ProcessDataCollector::Shutdown(); }

 protected:
  // Faked process data.
  // The process information will be truncated a lot, since most of the data in
  // a typical /proc/%u directory is unnecessary for testing.

  // Below are processes of different kinds: Crostini, ARC, Base and System,
  // both of which are system processes but Base corresponds to the root of the
  // process hierarchy, and Chrome (corresponding to separate types of processes
  // in |ProcessDataCollector::ProcessType|. These all contain trimmed down
  // information from various files in procfs.
  const std::vector<ProcessTestData> kAllProcessTestData = {
      ProcessTestData(ProcessData(0, "", "", PowerConsumerType::SYSTEM),
                      0 /* ppid */,
                      0.02 /* cpu_usage_fraction */),
      ProcessTestData(
          ProcessData(1, "init", "/usr/bin/init\n", PowerConsumerType::SYSTEM),
          0 /* ppid */,
          0.01 /* cpu_usage_fraction */),
      ProcessTestData(ProcessData(2,
                                  "chrome",
                                  "/opt/google/chrome/chrome\n",
                                  PowerConsumerType::CHROME),
                      1 /* ppid */,
                      0.02 /* cpu_usage_fraction */),
      ProcessTestData(
          ProcessData(
              3,
              "chrome",
              "/opt/google/chrome/chrome --other --commandline --options\n",
              PowerConsumerType::CHROME),
          1 /* ppid */,
          0.02 /* cpu_usage_fraction */),
      ProcessTestData(
          ProcessData(4,
                      "chrome",
                      "/opt/google/chrome/chrome --even --more --commandline"
                      "--options\n",
                      PowerConsumerType::CHROME),
          2 /* ppid */,
          0.03 /* cpu_usage_fraction */),
      ProcessTestData(ProcessData(5,
                                  "chrome",
                                  "/opt/google/chrome/chrome\n",
                                  PowerConsumerType::CHROME),
                      1 /* ppid */,
                      0.02 /* cpu_usage_fraction */),
      ProcessTestData(ProcessData(6,
                                  "vm_concierge",
                                  "/usr/bin/vm_concierge\n",
                                  PowerConsumerType::CROSTINI),
                      1 /* ppid */,
                      0.1 /* cpu_usage_fraction */),
      ProcessTestData(ProcessData(7,
                                  "some_container",
                                  "some_container\n",
                                  PowerConsumerType::CROSTINI),
                      6 /* ppid */,
                      0.03 /* cpu_usage_fraction */),
      ProcessTestData(ProcessData(8,
                                  "nested_under_some_container",
                                  "nested_under_some_container\n",
                                  PowerConsumerType::CROSTINI),
                      7 /* ppid */,
                      0.04 /* cpu_usage_fraction */),
      ProcessTestData(ProcessData(9,
                                  "other_container",
                                  "other_container\n",
                                  PowerConsumerType::CROSTINI),
                      6 /* ppid */,
                      0.02 /* cpu_usage_fraction */),
      ProcessTestData(
          ProcessData(10, "init", "/init --android\n", PowerConsumerType::ARC),
          1 /* ppid */,
          0.02 /* cpu_usage_fraction */),
      ProcessTestData(ProcessData(11,
                                  "org.google.chromium.1",
                                  "org.google.chromium.1\n",
                                  PowerConsumerType::ARC),
                      10 /* ppid */,
                      0.02 /* cpu_usage_fraction */),
      ProcessTestData(ProcessData(12,
                                  "org.google.chromium.2",
                                  "org.google.chromium.2\n",
                                  PowerConsumerType::ARC),
                      11 /* ppid */,
                      0.02 /* cpu_usage_fraction */),
      ProcessTestData(ProcessData(13,
                                  "org.google.chromium.3",
                                  "org.google.chromium.3\n",
                                  PowerConsumerType::ARC),
                      10 /* ppid */,
                      0.02 /* cpu_usage_fraction */),
      ProcessTestData(
          ProcessData(14, "powerd", "powerd\n", PowerConsumerType::SYSTEM),
          1 /* ppid */,
          0.02 /* cpu_usage_fraction */),
      ProcessTestData(ProcessData(15,
                                  "powerd_loggerd",
                                  "powerd_loggerd\n",
                                  PowerConsumerType::SYSTEM),
                      14 /* ppid */,
                      0.02 /* cpu_usage_fraction */),
      ProcessTestData(
          ProcessData(16, "garcon", "garcon\n", PowerConsumerType::SYSTEM),
          1 /* ppid */,
          0.02 /* cpu_usage_fraction */),
      ProcessTestData(ProcessData(17, "", "\n", PowerConsumerType::SYSTEM),
                      1 /* ppid */,
                      0.02 /* cpu_usage_fraction */)};

  // Helper function to generate a |Config| struct that will be passed to the
  // |ProcessDataCollector| to initialize it with an appropriate testing
  // configuration.
  Config CreateConfig(const base::FilePath& proc_path,
                      Config::AveragingTechnique technique);

  // A helper function that wraps |ValidateProcessList| for a convenient
  // testing setup.
  void RunTest(const std::vector<TimeStepExpectedResult>& expected_results,
               Config::AveragingTechnique averaging_technique);

  // Contains the PIDs of all the valid processes from |kProcessTestData|.
  std::unordered_set<pid_t> all_valid_pids_;

  // Represents the root of the simulated procfs.
  base::ScopedTempDir proc_dir_;

  base::test::TaskEnvironment task_environment_;

  // The current time step, used in testing functions.
  int timestep_ = 0;

  // Maps each PID to its respective |ProcessTestData| struct.
  std::unordered_map<pid_t, ProcessTestData> pid_infos_;

  // Contains the normalized averages of all the valid processes.
  std::unordered_map<pid_t, double> all_process_averages_;

 private:
  // Initialize relevant statistics like process averages.
  void InitializeStats();

  // Checks whether all of the processes that are returned from
  // |ProcessDataCollector::GetProcessUsages| are valid.
  void CheckValidProcesses(const std::vector<ProcessUsageData>& process_list,
                           const std::unordered_set<pid_t>& expected_pids);

  // Checks whether all given processes match their expected results. This
  // should only be called after |CheckValidProcesses|.
  void CheckProcessUsages(const std::vector<ProcessUsageData>& process_list,
                          const TimeStepExpectedResult& timestep_info);

  // Makes sure that the calculated results match the expected results.
  // |timestep_info| represents the expected results for a single time step.
  // |process_list| contains the calculated results from the
  // |ProcessDataCollector| which needs to compared against |timestep_info|.
  void ValidateProcessList(const TimeStepExpectedResult& timestep_info,
                           const std::vector<ProcessUsageData>& process_list);

  DISALLOW_COPY_AND_ASSIGN(ProcessDataCollectorTest);
};

ProcessDataCollector::Config ProcessDataCollectorTest::CreateConfig(
    const base::FilePath& proc_path,
    ProcessDataCollector::Config::AveragingTechnique technique) {
  std::string cmdline_fmt = proc_path.value() + "/%u/cmdline";
  std::string stat_fmt = proc_path.value() + "/%u/stat";
  std::string comm_fmt = proc_path.value() + "/%u/comm";
  // The delay is arbitrary and shouldn't actually be used since the
  // |ProcessDataCollector| will be initialized with |InitializeForTesting|.
  return ProcessDataCollector::Config(
      proc_path, proc_path.AppendASCII(kCustomCpuTimeFile),
      proc_path.AppendASCII(kCustomAndroidInitPidFile), cmdline_fmt, stat_fmt,
      comm_fmt, base::TimeDelta(), technique);
}

void ProcessDataCollectorTest::RunTest(
    const std::vector<TimeStepExpectedResult>& expected_results,
    Config::AveragingTechnique averaging_technique) {
  // Initialize the testing environment with all processes for the 0th time
  // step.
  SetUpProcfs(proc_dir_.GetPath(), kAllProcessTestData, 0);

  ProcessDataCollector::Config config =
      CreateConfig(proc_dir_.GetPath(), averaging_technique);

  ProcessDataCollector::InitializeForTesting(config);
  ProcessDataCollector* process_data_collector = ProcessDataCollector::Get();

  for (timestep_ = 0; timestep_ < static_cast<int>(expected_results.size());
       timestep_++) {
    // Sample from the simulated procfs.
    process_data_collector->SampleCpuUsageForTesting();

    // Set up the next iteration of the procfs.
    SetUpProcfs(proc_dir_.GetPath(), kAllProcessTestData, timestep_ + 1);

    // If the 0th |timestep_| was just sampled, then there are no results to
    // check. Just go to the next iteration. At least 2 samples from the
    // simulated procfs need to be gathered before an average can be calculated.
    if (timestep_ == 0)
      continue;

    const TimeStepExpectedResult& expected_result = expected_results[timestep_];
    const std::vector<ProcessDataCollector::ProcessUsageData>& process_list =
        process_data_collector->GetProcessUsages();
    ValidateProcessList(expected_result, process_list);
  }
}

void ProcessDataCollectorTest::InitializeStats() {
  // Map each PID to a |ProcessTestData| and gather the PIDs of all the valid
  // processes.
  for (const auto& process_info : kAllProcessTestData) {
    pid_infos_[process_info.process_data.pid] = process_info;
    if (!process_info.process_data.name.empty() &&
        !process_info.process_data.cmdline.empty()) {
      all_valid_pids_.insert(process_info.process_data.pid);
    }
  }

  // Make sure that the given fractions sum to less than 100% of the total CPU
  // time.
  double total_cpu_usage = 0.;
  for (const auto& process_info : kAllProcessTestData)
    total_cpu_usage += process_info.cpu_usage_fraction;
  ASSERT_LE(total_cpu_usage, 1.);

  // The average CPU usage for the first time step for each process.
  for (const auto& process_info : kAllProcessTestData)
    all_process_averages_[process_info.process_data.pid] =
        process_info.cpu_usage_fraction;

  // Normalize the averages for the first time step for a normal averaging
  // technique.
  NormalizeProcessAverages(all_valid_pids_, &all_process_averages_);
}

void ProcessDataCollectorTest::CheckValidProcesses(
    const std::vector<ProcessUsageData>& process_list,
    const std::unordered_set<pid_t>& expected_pids) {
  std::unordered_set<pid_t> calculated_pids;
  for (const auto& process : process_list)
    calculated_pids.insert(process.process_data.pid);
  ASSERT_EQ(expected_pids, calculated_pids);
}

void ProcessDataCollectorTest::CheckProcessUsages(
    const std::vector<ProcessUsageData>& process_list,
    const TimeStepExpectedResult& timestep_info) {
  for (const auto& process : process_list) {
    pid_t pid = process.process_data.pid;

    EXPECT_DOUBLE_EQ(process.power_usage_fraction,
                     timestep_info.expected_averages.at(pid))
        << "For PID " << pid
        << " the expected and calculated average CPU "
           "usages didn't match.";

    const ProcessTestData& process_info = timestep_info.pid_infos.at(pid);

    // Make sure that the |ProcessDataCollector::PowerConsumerType|s match.
    EXPECT_EQ(process.process_data.type, process_info.process_data.type)
        << "For PID " << pid
        << " the expected and calculated process types "
           "didn't match.";

    // Make sure the process name matches what is expected.
    EXPECT_EQ(process_info.process_data.name, process.process_data.name)
        << "For PID " << pid
        << " the expected and calculated process names "
           "didn't match.";
  }
}

void ProcessDataCollectorTest::ValidateProcessList(
    const TimeStepExpectedResult& timestep_info,
    const std::vector<ProcessUsageData>& process_list) {
  CheckValidProcesses(process_list, timestep_info.expected_pids);

  CheckProcessUsages(process_list, timestep_info);
}

// Averages for one time step and checks normal averages.
TEST_F(ProcessDataCollectorTest, AveragingBasic) {
  std::vector<TimeStepExpectedResult> expected_results;
  // For each time step, insert a dummy set of expected results for the 0th time
  // step, since there will be no expected results.
  expected_results.push_back(TimeStepExpectedResult());
  expected_results.push_back(TimeStepExpectedResult(
      all_valid_pids_, all_process_averages_, pid_infos_));
  RunTest(expected_results, Config::AveragingTechnique::AVERAGE);
}

// Averages for one time steps and checks exponential averages.
TEST_F(ProcessDataCollectorTest, ExpAveragingBasic) {
  std::vector<TimeStepExpectedResult> expected_results;
  expected_results.push_back(TimeStepExpectedResult());
  expected_results.push_back(TimeStepExpectedResult(
      all_valid_pids_, all_process_averages_, pid_infos_));
  RunTest(expected_results, Config::AveragingTechnique::EXPONENTIAL);
}

// Averages for two time steps and checks both normal averages.
TEST_F(ProcessDataCollectorTest, AveragingMultistep) {
  std::vector<TimeStepExpectedResult> expected_results;
  expected_results.push_back(TimeStepExpectedResult());
  expected_results.push_back(TimeStepExpectedResult(
      all_valid_pids_, all_process_averages_, pid_infos_));
  expected_results.push_back(TimeStepExpectedResult(
      all_valid_pids_, all_process_averages_, pid_infos_));
  RunTest(expected_results, Config::AveragingTechnique::AVERAGE);
}

// Averages for two time steps and checks exponential moving averages.
TEST_F(ProcessDataCollectorTest, ExpAveragingMultistep) {
  std::vector<TimeStepExpectedResult> expected_results;
  expected_results.push_back(TimeStepExpectedResult());
  expected_results.push_back(TimeStepExpectedResult(
      all_valid_pids_, all_process_averages_, pid_infos_));
  expected_results.push_back(TimeStepExpectedResult(
      all_valid_pids_, all_process_averages_, pid_infos_));
  RunTest(expected_results, Config::AveragingTechnique::EXPONENTIAL);
}

}  // namespace chromeos
