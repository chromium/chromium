// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "chrome/browser/ash/power/cpu_data_collector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// The suffix path of fake CPU frequency file for cpu0.
constexpr char kTimeInStateSuffixPathCpu0[] = "cpu0/time_in_state";

// The suffix path of fake CPU frequency file for cpu1.
constexpr char kTimeInStateSuffixPathCpu1[] = "cpu1/time_in_state";

// The suffix path of fake CPU frequency file for all cpus.
constexpr char kAllTimeInStateSuffixPath[] = "all_time_in_state";

// The string content of the fake CPU frequency file for cpu0.
constexpr char kTimeInStateContentCpu0[] =
    "20000 30000000\n"
    "60000 90000\n"
    "100000 50000\n";

// The string content of the fake CPU frequency file for cpu1.
constexpr char kTimeInStateContentCpu1[] =
    "20000 31000000\n"
    "60000 91000\n"
    "100000 51000\n";

// The string content of the fake CPU frequency file for all CPUs.
constexpr char kAllTimeInStateContent[] =
    "freq\t\tcpu0\t\tcpu1\t\t\n"
    "20000\t\t30000000\t\t31000000\t\t\n"
    "60000\t\t90000\t\t91000\t\t\n"
    "100000\t\t50000\t\t51000\t\t\n";

// The string content of the fake CPU frequency file for all CPUs except one
// that reports "N/A" as the CPU/freq combinations that are invalid.
constexpr char kAllTimeInStateContentNA[] =
    "freq\t\tcpu0\t\tcpu1\t\tcpu2\t\t\n"
    "20000\t\t30000000\t\t31000000\t\tN/A\t\t\n"
    "60000\t\t90000\t\t91000\t\tN/A\t\t\n"
    "100000\t\t50000\t\t51000\t\tN/A\t\t\n";

// The string content of the fake CPU frequency file for all CPUs except one
// that does not report freq as the CPU is turned off.
constexpr char kAllTimeInStateContentOff[] =
    "freq\t\tcpu0\t\tcpu1\t\tcpu2\t\t\n"
    "20000\t\t30000000\t\t31000000\t\t\n"
    "60000\t\t90000\t\t91000\t\t\n"
    "100000\t\t50000\t\t51000\t\t\n";

}  // namespace

class CpuDataCollectorTest : public testing::Test {
 public:
  CpuDataCollectorTest()
      : kExpectedCpuFreqStateNames({"20", "60", "100"}),
        kExpectedTimeInStateCpu0({base::Milliseconds(300000000),
                                  base::Milliseconds(900000),
                                  base::Milliseconds(500000)}),
        kExpectedTimeInStateCpu1({base::Milliseconds(310000000),
                                  base::Milliseconds(910000),
                                  base::Milliseconds(510000)}) {}

  CpuDataCollectorTest(const CpuDataCollectorTest&) = delete;
  CpuDataCollectorTest& operator=(const CpuDataCollectorTest&) = delete;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    time_in_state_path_cpu0_ =
        temp_dir_.GetPath().AppendASCII(kTimeInStateSuffixPathCpu0);
    time_in_state_path_cpu1_ =
        temp_dir_.GetPath().AppendASCII(kTimeInStateSuffixPathCpu1);
    all_time_in_state_path_ =
        temp_dir_.GetPath().AppendASCII(kAllTimeInStateSuffixPath);
  }

 protected:
  // Expected CPU frequency state names after calling |ReadCpuFreqTimeInState|
  // or |ReadCpuFreqAllTimeInState|
  const std::vector<std::string> kExpectedCpuFreqStateNames;

  // Expected time_in_state of sample for cpu0.
  const std::vector<base::TimeDelta> kExpectedTimeInStateCpu0;

  // Expected time_in_state of sample for cpu1.
  const std::vector<base::TimeDelta> kExpectedTimeInStateCpu1;

  base::ScopedTempDir temp_dir_;
  base::FilePath time_in_state_path_cpu0_;
  base::FilePath time_in_state_path_cpu1_;
  base::FilePath all_time_in_state_path_;
};

TEST_F(CpuDataCollectorTest, ReadCpuFreqTimeInState) {
  ASSERT_TRUE(base::CreateTemporaryFile(&time_in_state_path_cpu0_));
  ASSERT_TRUE(base::CreateTemporaryFile(&time_in_state_path_cpu1_));
  ASSERT_TRUE(
      base::WriteFile(time_in_state_path_cpu0_, kTimeInStateContentCpu0));
  ASSERT_TRUE(
      base::WriteFile(time_in_state_path_cpu1_, kTimeInStateContentCpu1));

  std::vector<std::string> cpu_freq_state_names;
  CpuDataCollector::StateOccupancySample freq_sample_cpu0;
  CpuDataCollector::StateOccupancySample freq_sample_cpu1;

  CpuDataCollector::ReadCpuFreqTimeInState(
      time_in_state_path_cpu0_, &cpu_freq_state_names, &freq_sample_cpu0);
  EXPECT_EQ(kExpectedCpuFreqStateNames, cpu_freq_state_names);
  EXPECT_EQ(kExpectedTimeInStateCpu0, freq_sample_cpu0.time_in_state);

  cpu_freq_state_names.clear();
  CpuDataCollector::ReadCpuFreqTimeInState(
      time_in_state_path_cpu1_, &cpu_freq_state_names, &freq_sample_cpu1);
  EXPECT_EQ(kExpectedCpuFreqStateNames, cpu_freq_state_names);
  EXPECT_EQ(kExpectedTimeInStateCpu1, freq_sample_cpu1.time_in_state);
}

TEST_F(CpuDataCollectorTest, ReadCpuFreqAllTimeInState) {
  ASSERT_TRUE(base::CreateTemporaryFile(&all_time_in_state_path_));
  ASSERT_TRUE(base::WriteFile(all_time_in_state_path_, kAllTimeInStateContent));

  std::vector<std::string> cpu_freq_state_names;
  std::vector<CpuDataCollector::StateOccupancySample> freq_samples;
  CpuDataCollector::StateOccupancySample freq_sample_cpu0;
  CpuDataCollector::StateOccupancySample freq_sample_cpu1;
  // |ReadCpuFreqAllTimeInState| only collects samples for CPUs that are online.
  freq_sample_cpu0.cpu_online = true;
  freq_sample_cpu1.cpu_online = true;
  freq_samples.push_back(freq_sample_cpu0);
  freq_samples.push_back(freq_sample_cpu1);

  CpuDataCollector::ReadCpuFreqAllTimeInState(
      2, all_time_in_state_path_, &cpu_freq_state_names, &freq_samples);
  EXPECT_EQ(kExpectedCpuFreqStateNames, cpu_freq_state_names);
  EXPECT_EQ(kExpectedTimeInStateCpu0, freq_samples[0].time_in_state);
  EXPECT_EQ(kExpectedTimeInStateCpu1, freq_samples[1].time_in_state);
}

TEST_F(CpuDataCollectorTest, ReadCpuFreqAllTimeInStateNA) {
  ASSERT_TRUE(base::CreateTemporaryFile(&all_time_in_state_path_));
  ASSERT_TRUE(
      base::WriteFile(all_time_in_state_path_, kAllTimeInStateContentNA));

  std::vector<std::string> cpu_freq_state_names;
  std::vector<CpuDataCollector::StateOccupancySample> freq_samples;
  CpuDataCollector::StateOccupancySample freq_sample_cpu0;
  CpuDataCollector::StateOccupancySample freq_sample_cpu1;
  CpuDataCollector::StateOccupancySample freq_sample_cpu2;
  // |ReadCpuFreqAllTimeInState| only collects samples for CPUs that are online.
  freq_sample_cpu0.cpu_online = true;
  freq_sample_cpu1.cpu_online = true;
  freq_sample_cpu2.cpu_online = true;
  freq_samples.push_back(freq_sample_cpu0);
  freq_samples.push_back(freq_sample_cpu1);
  freq_samples.push_back(freq_sample_cpu2);

  CpuDataCollector::ReadCpuFreqAllTimeInState(
      3, all_time_in_state_path_, &cpu_freq_state_names, &freq_samples);
  EXPECT_EQ(kExpectedCpuFreqStateNames, cpu_freq_state_names);
  EXPECT_EQ(kExpectedTimeInStateCpu0, freq_samples[0].time_in_state);
  EXPECT_EQ(kExpectedTimeInStateCpu1, freq_samples[1].time_in_state);
  EXPECT_TRUE(freq_samples[2].time_in_state.empty());
}

TEST_F(CpuDataCollectorTest, ReadCpuFreqAllTimeInStateOff) {
  ASSERT_TRUE(base::CreateTemporaryFile(&all_time_in_state_path_));
  ASSERT_TRUE(
      base::WriteFile(all_time_in_state_path_, kAllTimeInStateContentOff));

  std::vector<std::string> cpu_freq_state_names;
  std::vector<CpuDataCollector::StateOccupancySample> freq_samples;
  CpuDataCollector::StateOccupancySample freq_sample_cpu0;
  CpuDataCollector::StateOccupancySample freq_sample_cpu1;
  CpuDataCollector::StateOccupancySample freq_sample_cpu2;
  // |ReadCpuFreqAllTimeInState| only collects samples for CPUs that are online.
  freq_sample_cpu0.cpu_online = true;
  freq_sample_cpu1.cpu_online = true;
  freq_sample_cpu2.cpu_online = false;
  freq_samples.push_back(freq_sample_cpu0);
  freq_samples.push_back(freq_sample_cpu1);
  freq_samples.push_back(freq_sample_cpu2);

  CpuDataCollector::ReadCpuFreqAllTimeInState(
      2, all_time_in_state_path_, &cpu_freq_state_names, &freq_samples);
  EXPECT_EQ(kExpectedCpuFreqStateNames, cpu_freq_state_names);
  EXPECT_EQ(kExpectedTimeInStateCpu0, freq_samples[0].time_in_state);
  EXPECT_EQ(kExpectedTimeInStateCpu1, freq_samples[1].time_in_state);
  EXPECT_TRUE(freq_samples[2].time_in_state.empty());
}

}  // namespace ash
