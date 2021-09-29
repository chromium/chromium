// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/cpufreq_monitor_android.h"

#include <list>

#include <fcntl.h>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

class TestTaskRunner final : public SingleThreadTaskRunner {
 public:
  bool PostDelayedTask(const Location& from_here,
                       OnceClosure task,
                       base::TimeDelta delay) override {
    delayed_tasks_.push_back(std::make_pair(std::move(delay), std::move(task)));
    return true;
  }

  bool PostNonNestableDelayedTask(const Location& from_here,
                                  OnceClosure task,
                                  base::TimeDelta delay) override {
    NOTREACHED();
    return false;
  }

  bool RunsTasksInCurrentSequence() const override { return true; }

  // Returns the delay in ms for this task if there was a task to be run,
  // and -1 if there are no tasks in the queue.
  int64_t RunNextTask() {
    if (delayed_tasks_.size() == 0)
      return -1;
    auto time_delta = delayed_tasks_.front().first;
    std::move(delayed_tasks_.front().second).Run();
    delayed_tasks_.pop_front();
    return time_delta.InMilliseconds();
  }

 private:
  ~TestTaskRunner() override {}

  std::list<std::pair<base::TimeDelta, OnceClosure>> delayed_tasks_;
};

class TestDelegate : public CPUFreqMonitorDelegate {
 public:
  TestDelegate(const std::string& temp_dir_path)
      : temp_dir_path_(temp_dir_path) {}

  void set_trace_category_enabled(bool enabled) {
    trace_category_enabled_ = enabled;
  }

  void set_cpu_ids(const std::vector<unsigned int>& cpu_ids) {
    cpu_ids_ = cpu_ids;
  }

  void set_kernel_max_cpu(unsigned int kernel_max_cpu) {
    kernel_max_cpu_ = kernel_max_cpu;
  }

  const std::vector<std::pair<unsigned int, unsigned int>>& recorded_freqs() {
    return recorded_freqs_;
  }

  // CPUFreqMonitorDelegate implementation:
  void GetCPUIds(std::vector<unsigned int>* ids) const override {
    // Use the test values if available.
    if (cpu_ids_.size() > 0) {
      *ids = cpu_ids_;
      return;
    }
    // Otherwise fall back to the original function.
    CPUFreqMonitorDelegate::GetCPUIds(ids);
  }

  void RecordFrequency(unsigned int cpu_id, unsigned int freq) override {
    recorded_freqs_.emplace_back(
        std::pair<unsigned int, unsigned int>(cpu_id, freq));
  }

  bool IsTraceCategoryEnabled() const override {
    return trace_category_enabled_;
  }

  std::string GetScalingCurFreqPathString(unsigned int cpu_id) const override {
    return base::StringPrintf("%s/scaling_cur_freq%d", temp_dir_path_.c_str(),
                              cpu_id);
  }

  std::string GetRelatedCPUsPathString(unsigned int cpu_id) const override {
    return base::StringPrintf("%s/related_cpus%d", temp_dir_path_.c_str(),
                              cpu_id);
  }

  unsigned int GetKernelMaxCPUs() const override { return kernel_max_cpu_; }

 protected:
  scoped_refptr<SingleThreadTaskRunner> CreateTaskRunner() override {
    return base::WrapRefCounted(new TestTaskRunner());
  }

 private:
  // Maps CPU ID to frequency.
  std::vector<std::pair<unsigned int, unsigned int>> recorded_freqs_;

  std::vector<unsigned int> cpu_ids_;

  bool trace_category_enabled_ = true;
  std::string temp_dir_path_;
  unsigned int kernel_max_cpu_ = 0;
};

class CPUFreqMonitorTest : public testing::Test {
 public:
  CPUFreqMonitorTest() : testing::Test() {}

  void SetUp() override {
    temp_dir_ = std::make_unique<ScopedTempDir>();
    ASSERT_TRUE(temp_dir_->CreateUniqueTempDir());

    std::string base_path = temp_dir_->GetPath().value();
    auto delegate = std::make_unique<TestDelegate>(base_path);
    // Retain a pointer to the delegate since we're passing ownership to the
    // monitor but we need to be able to modify it.
    delegate_ = delegate.get();

    // Can't use make_unique because it's a private constructor.
    CPUFreqMonitor* monitor = new CPUFreqMonitor(std::move(delegate));
    monitor_.reset(monitor);
  }

  void TearDown() override {
    monitor_.reset();
    temp_dir_.reset();
  }

  void CreateDefaultScalingCurFreqFiles(
      const std::vector<std::pair<unsigned int, unsigned int>>& frequencies) {
    for (auto& pair : frequencies) {
      std::string file_path =
          delegate_->GetScalingCurFreqPathString(pair.first);
      std::string str_freq = base::StringPrintf("%d\n", pair.second);
      base::WriteFile(base::FilePath(file_path), str_freq);
    }
  }

  void CreateRelatedCPUFiles(const std::vector<unsigned int>& clusters,
                             const std::vector<std::string>& related_cpus) {
    for (unsigned int i = 0; i < clusters.size(); i++) {
      base::WriteFile(base::FilePath(delegate_->GetRelatedCPUsPathString(i)),
                      related_cpus[clusters[i]]);
    }
  }

  void InitBasicCPUInfo() {
    std::vector<std::pair<unsigned int, unsigned int>> frequencies = {
        {0, 500}, {2, 1000}, {4, 800}, {6, 750},
    };
    std::vector<unsigned int> cpu_ids;
    for (auto& pair : frequencies) {
      cpu_ids.push_back(pair.first);
    }
    delegate()->set_cpu_ids(cpu_ids);

    CreateDefaultScalingCurFreqFiles(frequencies);
  }

  TestTaskRunner* GetOrCreateTaskRunner() {
    return static_cast<TestTaskRunner*>(
        monitor_->GetOrCreateTaskRunner().get());
  }

  CPUFreqMonitor* monitor() { return monitor_.get(); }
  ScopedTempDir* temp_dir() { return temp_dir_.get(); }
  TestDelegate* delegate() { return delegate_; }

 private:
  scoped_refptr<TestTaskRunner> task_runner_;
  std::unique_ptr<ScopedTempDir> temp_dir_;
  std::unique_ptr<CPUFreqMonitor> monitor_;
  TestDelegate* delegate_;
};

TEST_F(CPUFreqMonitorTest, TestStart) {
  InitBasicCPUInfo();
  monitor()->Start();
  ASSERT_TRUE(monitor()->IsEnabledForTesting());
}

TEST_F(CPUFreqMonitorTest, TestSample) {
  // Vector of CPU ID to frequency.
  std::vector<std::pair<unsigned int, unsigned int>> frequencies = {{0, 500},
                                                                    {4, 1000}};
  std::vector<unsigned int> cpu_ids;
  for (auto& pair : frequencies) {
    cpu_ids.push_back(pair.first);
  }
  delegate()->set_cpu_ids(cpu_ids);

  // Build some files with CPU frequency info in it to sample.
  std::vector<std::pair<unsigned int, base::ScopedFD>> fds;
  for (auto& pair : frequencies) {
    std::string file_path = base::StringPrintf(
        "%s/temp%d", temp_dir()->GetPath().value().c_str(), pair.first);

    // Uses raw file descriptors so we can build our ScopedFDs in the same loop.
    int fd = open(file_path.c_str(), O_RDWR | O_CREAT | O_SYNC,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    ASSERT_FALSE(fd == -1);

    std::string str_freq = base::StringPrintf("%d\n", pair.second);
    write(fd, str_freq.c_str(), str_freq.length());
    fds.emplace_back(std::make_pair(pair.first, base::ScopedFD(fd)));
  }

  // This ensures we set it to enabled before sampling, otherwise the call to
  // Sample() will end early.
  CreateDefaultScalingCurFreqFiles(frequencies);
  monitor()->Start();
  ASSERT_TRUE(monitor()->IsEnabledForTesting());

  // Ensure that we run our undelayed posted task for Sample.
  ASSERT_EQ(GetOrCreateTaskRunner()->RunNextTask(), 0);
  // Run the new delayed task so we sample again.
  ASSERT_TRUE(GetOrCreateTaskRunner()->RunNextTask() ==
              CPUFreqMonitor::kDefaultCPUFreqSampleIntervalMs);

  // Ensure that the values that we recorded agree with the frequencies above.
  auto recorded_freqs = delegate()->recorded_freqs();
  ASSERT_EQ(recorded_freqs.size(), frequencies.size() * 2);
  for (unsigned int i = 0; i < frequencies.size(); i++) {
    auto freq_pair = frequencies[i];
    // We sampled twice, so the recording pairs should be equal.
    auto recorded_pair_1 = recorded_freqs[i];
    auto recorded_pair_2 = recorded_freqs[i + 2];
    ASSERT_EQ(freq_pair.first, recorded_pair_1.first);
    ASSERT_EQ(freq_pair.second, recorded_pair_1.second);
    ASSERT_EQ(freq_pair.first, recorded_pair_2.first);
    ASSERT_EQ(freq_pair.second, recorded_pair_2.second);
  }

  // Test that calling Stop works, we shouldn't post any more tasks if Sample
  // is called.
  monitor()->Stop();
  // Clear out the first Sample task that's on deck, then try again to make sure
  // no new task was posted.
  ASSERT_TRUE(GetOrCreateTaskRunner()->RunNextTask() ==
              CPUFreqMonitor::kDefaultCPUFreqSampleIntervalMs);
  ASSERT_EQ(GetOrCreateTaskRunner()->RunNextTask(), -1);
}

TEST_F(CPUFreqMonitorTest, TestStartFail_TraceCategoryDisabled) {
  delegate()->set_trace_category_enabled(false);
  CreateDefaultScalingCurFreqFiles({{0, 1000}});
  monitor()->Start();
  ASSERT_FALSE(monitor()->IsEnabledForTesting());
}

TEST_F(CPUFreqMonitorTest, TestStartFail_NoScalingCurFreqFiles) {
  monitor()->Start();
  ASSERT_FALSE(monitor()->IsEnabledForTesting());
}

TEST_F(CPUFreqMonitorTest, TestDelegate_GetCPUIds) {
  delegate()->set_kernel_max_cpu(8);
  std::vector<std::string> related_cpus = {"0 1 2 3\n", "4 5 6 7\n"};
  std::vector<unsigned int> clusters = {0, 0, 0, 0, 1, 1, 1, 1};

  CreateRelatedCPUFiles(clusters, related_cpus);

  std::vector<unsigned int> cpu_ids;
  delegate()->GetCPUIds(&cpu_ids);
  EXPECT_EQ(cpu_ids.size(), 2U);
  EXPECT_EQ(cpu_ids[0], 0U);
  EXPECT_EQ(cpu_ids[1], 4U);
}

TEST_F(CPUFreqMonitorTest, TestDelegate_GetCPUIds_FailReadingFallback) {
  delegate()->set_kernel_max_cpu(8);

  std::vector<unsigned int> cpu_ids;
  delegate()->GetCPUIds(&cpu_ids);
  EXPECT_EQ(cpu_ids.size(), 1U);
  EXPECT_EQ(cpu_ids[0], 0U);
}

TEST_F(CPUFreqMonitorTest, TestMultipleStartStop) {
  InitBasicCPUInfo();

  monitor()->Start();
  ASSERT_TRUE(monitor()->IsEnabledForTesting());
  monitor()->Stop();
  ASSERT_FALSE(monitor()->IsEnabledForTesting());

  monitor()->Start();
  ASSERT_TRUE(monitor()->IsEnabledForTesting());
  monitor()->Stop();
  ASSERT_FALSE(monitor()->IsEnabledForTesting());
}

TEST_F(CPUFreqMonitorTest, TestTraceLogEnableDisable) {
  InitBasicCPUInfo();

  monitor()->OnTraceLogEnabled();
  // OnTraceLogEnabled posts a task for Start.
  GetOrCreateTaskRunner()->RunNextTask();
  ASSERT_TRUE(monitor()->IsEnabledForTesting());
  monitor()->OnTraceLogDisabled();
  ASSERT_FALSE(monitor()->IsEnabledForTesting());
  // We also need to clear out the task for Sample from the Start call.
  GetOrCreateTaskRunner()->RunNextTask();

  monitor()->OnTraceLogEnabled();
  GetOrCreateTaskRunner()->RunNextTask();
  ASSERT_TRUE(monitor()->IsEnabledForTesting());
  monitor()->OnTraceLogDisabled();
  ASSERT_FALSE(monitor()->IsEnabledForTesting());
}

}  // namespace trace_event
}  // namespace base
