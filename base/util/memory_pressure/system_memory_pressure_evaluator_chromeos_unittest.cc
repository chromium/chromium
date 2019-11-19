// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/memory_pressure/system_memory_pressure_evaluator_chromeos.h"

#include <unistd.h>
#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace util {
namespace chromeos {

namespace {
bool SetFileContents(const base::FilePath& path, const std::string& contents) {
  return static_cast<std::string::size_type>(base::WriteFile(
             path, contents.c_str(), contents.size())) == contents.size();
}

// Since it would be very hard to mock sysfs instead we will send in our own
// implementation of WaitForKernelNotification which instead will block on a
// pipe that we can trigger for the test to cause a mock kernel notification.
bool WaitForMockKernelNotification(int pipe_read_fd, int available_fd) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  // We just use a pipe to block our kernel notification thread until we have
  // a fake kernel notification.
  char buf = 0;
  int res = HANDLE_EINTR(read(pipe_read_fd, &buf, sizeof(buf)));

  // Fail if we encounter any error.
  return res > 0;
}

void TriggerKernelNotification(int pipe_write_fd) {
  char buf = '1';
  HANDLE_EINTR(write(pipe_write_fd, &buf, sizeof(buf)));
}

// Processes OnMemoryPressure calls by just storing the sequence of events so we
// can validate that we received the expected pressure levels as the test runs.
void OnMemoryPressure(
    std::vector<base::MemoryPressureListener::MemoryPressureLevel>* history,
    base::MemoryPressureListener::MemoryPressureLevel level) {
  history->push_back(level);
}

void RunLoopRunWithTimeout(base::TimeDelta timeout) {
  base::RunLoop run_loop;
  base::RunLoop::ScopedRunTimeoutForTest run_timeout(timeout,
                                                     run_loop.QuitClosure());
  run_loop.Run();
}
}  // namespace

class TestSystemMemoryPressureEvaluator : public SystemMemoryPressureEvaluator {
 public:
  TestSystemMemoryPressureEvaluator(
      const std::string& mock_margin_file,
      const std::string& mock_available_file,
      base::RepeatingCallback<bool(int)> kernel_waiting_callback,
      bool enable_metrics,
      std::unique_ptr<MemoryPressureVoter> voter)
      : SystemMemoryPressureEvaluator(mock_margin_file,
                                      mock_available_file,
                                      std::move(kernel_waiting_callback),
                                      enable_metrics,
                                      std::move(voter)) {}

  static std::vector<int> GetMarginFileParts(const std::string& file) {
    return SystemMemoryPressureEvaluator::GetMarginFileParts(file);
  }

  ~TestSystemMemoryPressureEvaluator() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSystemMemoryPressureEvaluator);
};

TEST(ChromeOSSystemMemoryPressureEvaluatorTest, ParseMarginFileGood) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());

  base::FilePath margin_file = tmp_dir.GetPath().Append("margin");

  ASSERT_TRUE(SetFileContents(margin_file, "123"));
  const std::vector<int> parts1 =
      TestSystemMemoryPressureEvaluator::GetMarginFileParts(
          margin_file.value());
  ASSERT_EQ(1u, parts1.size());
  ASSERT_EQ(123, parts1[0]);

  ASSERT_TRUE(SetFileContents(margin_file, "123 456"));
  const std::vector<int> parts2 =
      TestSystemMemoryPressureEvaluator::GetMarginFileParts(
          margin_file.value());
  ASSERT_EQ(2u, parts2.size());
  ASSERT_EQ(123, parts2[0]);
  ASSERT_EQ(456, parts2[1]);
}

TEST(ChromeOSSystemMemoryPressureEvaluatorTest, ParseMarginFileBad) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::FilePath margin_file = tmp_dir.GetPath().Append("margin");

  // An empty margin file is bad.
  ASSERT_TRUE(SetFileContents(margin_file, ""));
  ASSERT_TRUE(
      TestSystemMemoryPressureEvaluator::GetMarginFileParts(margin_file.value())
          .empty());

  // The numbers will be in base10, so 4a6 would be invalid.
  ASSERT_TRUE(SetFileContents(margin_file, "123 4a6"));
  ASSERT_TRUE(
      TestSystemMemoryPressureEvaluator::GetMarginFileParts(margin_file.value())
          .empty());

  // The numbers must be integers.
  ASSERT_TRUE(SetFileContents(margin_file, "123.2 412.3"));
  ASSERT_TRUE(
      TestSystemMemoryPressureEvaluator::GetMarginFileParts(margin_file.value())
          .empty());
}

TEST(ChromeOSSystemMemoryPressureEvaluatorTest, CheckMemoryPressure) {
  // Create a temporary directory for our margin and available files.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());

  base::FilePath margin_file = tmp_dir.GetPath().Append("margin");
  base::FilePath available_file = tmp_dir.GetPath().Append("available");

  // Set the margin values to 500 (critical) and 1000 (moderate).
  const std::string kMarginContents = "500 1000";
  ASSERT_TRUE(SetFileContents(margin_file, kMarginContents));

  // Write the initial available contents.
  const std::string kInitialAvailableContents = "1500";
  ASSERT_TRUE(SetFileContents(available_file, kInitialAvailableContents));

  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  // We will use a mock listener to keep track of our kernel notifications which
  // cause event to be fired. We can just examine the sequence of pressure
  // events when we're done to validate that the pressure events were as
  // expected.
  std::vector<base::MemoryPressureListener::MemoryPressureLevel>
      pressure_events;
  auto listener = std::make_unique<base::MemoryPressureListener>(
      base::BindRepeating(&OnMemoryPressure, &pressure_events));

  MultiSourceMemoryPressureMonitor monitor;
  monitor.ResetSystemEvaluatorForTesting();

  // We use a pipe to notify our blocked kernel notification thread that there
  // is a kernel notification we need to use a simple blocking syscall and
  // read(2)/write(2) will work.
  int fds[2] = {};
  ASSERT_EQ(0, HANDLE_EINTR(pipe(fds)));

  // Make sure the pipe FDs get closed.
  base::ScopedFD write_end(fds[1]);
  base::ScopedFD read_end(fds[0]);

  auto evaluator = std::make_unique<TestSystemMemoryPressureEvaluator>(
      margin_file.value(), available_file.value(),
      // Bind the read end to WaitForMockKernelNotification.
      base::BindRepeating(&WaitForMockKernelNotification, read_end.get()),
      /*enable_metrics=*/false, monitor.CreateVoter());

  // Validate that our margin levels are as expected after being parsed from our
  // synthetic margin file.
  ASSERT_EQ(500, evaluator->CriticalPressureThresholdMBForTesting());
  ASSERT_EQ(1000, evaluator->ModeratePressureThresholdMBForTesting());

  // At this point we have no memory pressure.
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator->current_vote());

  // Moderate Pressure.
  ASSERT_TRUE(SetFileContents(available_file, "900"));
  TriggerKernelNotification(write_end.get());
  // TODO(bgeffon): Use RunLoop::QuitClosure() instead of relying on "spin for
  // 1 second".
  RunLoopRunWithTimeout(base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator->current_vote());

  // Critical Pressure.
  ASSERT_TRUE(SetFileContents(available_file, "450"));
  TriggerKernelNotification(write_end.get());
  RunLoopRunWithTimeout(base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator->current_vote());

  // Moderate Pressure.
  ASSERT_TRUE(SetFileContents(available_file, "550"));
  TriggerKernelNotification(write_end.get());
  RunLoopRunWithTimeout(base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator->current_vote());

  // No pressure, note: this will not cause any event.
  ASSERT_TRUE(SetFileContents(available_file, "1150"));
  TriggerKernelNotification(write_end.get());
  RunLoopRunWithTimeout(base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator->current_vote());

  // Back into moderate.
  ASSERT_TRUE(SetFileContents(available_file, "950"));
  TriggerKernelNotification(write_end.get());
  RunLoopRunWithTimeout(base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator->current_vote());

  // Now our events should be MODERATE, CRITICAL, MODERATE.
  ASSERT_EQ(4u, pressure_events.size());
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            pressure_events[0]);
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            pressure_events[1]);
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            pressure_events[2]);
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            pressure_events[3]);
}

}  // namespace chromeos
}  // namespace util
