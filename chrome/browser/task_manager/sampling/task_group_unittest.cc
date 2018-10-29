// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/sampling/task_group.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/task_manager/sampling/shared_sampler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "gpu/ipc/common/memory_stats.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_manager {

namespace {

class FakeTask : public Task {
 public:
  FakeTask(base::ProcessId process_id, Type type)
      : Task(base::string16(),
             "FakeTask",
             nullptr,
             base::kNullProcessHandle,
             process_id),
        type_(type) {}

  Type GetType() const override { return type_; }

  int GetChildProcessUniqueID() const override { return 0; }

  const Task* GetParentTask() const override { return nullptr; }

  SessionID GetTabId() const override { return SessionID::InvalidValue(); }

 private:
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(FakeTask);
};

}  // namespace

class TaskGroupTest : public testing::Test {
 public:
  TaskGroupTest()
      : io_task_runner_(base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::IO})),
        run_loop_(std::make_unique<base::RunLoop>()),
        task_group_(base::Process::Current().Handle(),
                    base::Process::Current().Pid(),
                    base::Bind(&TaskGroupTest::OnBackgroundCalculationsDone,
                               base::Unretained(this)),
                    new SharedSampler(io_task_runner_),
                    io_task_runner_),
        fake_task_(base::Process::Current().Pid(), Task::UNKNOWN) {
    // Refresh() is only valid on non-empty TaskGroups, so add a fake Task.
    task_group_.AddTask(&fake_task_);
  }

 protected:
  void OnBackgroundCalculationsDone() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    background_refresh_complete_ = true;
    run_loop_->QuitWhenIdle();
  }

  content::TestBrowserThreadBundle browser_threads_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  std::unique_ptr<base::RunLoop> run_loop_;
  TaskGroup task_group_;
  FakeTask fake_task_;
  bool background_refresh_complete_ = false;

  DISALLOW_COPY_AND_ASSIGN(TaskGroupTest);
};

// Verify that calling TaskGroup::Refresh() without specifying any fields to
// refresh trivially completes, without crashing or leaving things in a weird
// state.
TEST_F(TaskGroupTest, NullRefresh) {
  task_group_.Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(), 0);
  EXPECT_TRUE(task_group_.AreBackgroundCalculationsDone());
  EXPECT_FALSE(background_refresh_complete_);
}

// Ensure that refreshing an empty TaskGroup causes a DCHECK (if enabled).
TEST_F(TaskGroupTest, RefreshZeroTasksDeathTest) {
  // Remove the fake Task from the group.
  task_group_.RemoveTask(&fake_task_);

  EXPECT_DCHECK_DEATH(
      task_group_.Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(), 0));
}

// Verify that Refresh() for a field which can be refreshed synchronously
// completes immediately, without leaving any background calculations pending.
TEST_F(TaskGroupTest, SyncRefresh) {
  task_group_.Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(),
                      REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_TRUE(task_group_.AreBackgroundCalculationsDone());
  EXPECT_FALSE(background_refresh_complete_);
}

// Some fields are refreshed on a per-TaskGroup basis, but require asynchronous
// work (e.g. on another thread) to complete. Cpu is such a field, so verify
// that it is correctly reported as requiring background calculations.
TEST_F(TaskGroupTest, AsyncRefresh) {
  task_group_.Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(),
                      REFRESH_TYPE_CPU);
  EXPECT_FALSE(task_group_.AreBackgroundCalculationsDone());

  ASSERT_FALSE(background_refresh_complete_);
  run_loop_->Run();

  EXPECT_TRUE(task_group_.AreBackgroundCalculationsDone());
  EXPECT_TRUE(background_refresh_complete_);
}

// Some fields are refreshed system-wide, via a SharedSampler, which completes
// asynchronously. Idle wakeups are reported via SharedSampler on some systems
// and via asynchronous refresh on others, so we just test that that field
// requires background calculations, similarly to the AsyncRefresh test above.
TEST_F(TaskGroupTest, SharedAsyncRefresh) {
  task_group_.Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(),
                      REFRESH_TYPE_IDLE_WAKEUPS);
  EXPECT_FALSE(task_group_.AreBackgroundCalculationsDone());

  ASSERT_FALSE(background_refresh_complete_);
  run_loop_->Run();

  EXPECT_TRUE(background_refresh_complete_);

  EXPECT_TRUE(task_group_.AreBackgroundCalculationsDone());
}

// Ensure that if NaCl is enabled then calling Refresh with a NaCl Task active
// results in asynchronous completion. Also verifies that if NaCl is disabled
// then completion is synchronous.
TEST_F(TaskGroupTest, NaclRefreshWithTask) {
  FakeTask fake_task(base::Process::Current().Pid(), Task::NACL);
  task_group_.AddTask(&fake_task);

  task_group_.Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(),
                      REFRESH_TYPE_NACL);
#if BUILDFLAG(ENABLE_NACL)
  EXPECT_FALSE(task_group_.AreBackgroundCalculationsDone());

  ASSERT_FALSE(background_refresh_complete_);
  run_loop_->Run();

  EXPECT_TRUE(background_refresh_complete_);
#endif  // BUILDFLAG(ENABLE_NACL)

  EXPECT_TRUE(task_group_.AreBackgroundCalculationsDone());
}

// Test the task has correct network usage rate when zero bytes read and sent.
TEST_F(TaskGroupTest, NetworkBytesSentReadZero) {
  const int zero_bytes = 0;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER);
  fake_task.OnNetworkBytesRead(zero_bytes);
  fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                    REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(zero_bytes, fake_task.network_usage_rate());
  fake_task.OnNetworkBytesSent(zero_bytes);
  fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                    REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(zero_bytes, fake_task.network_usage_rate());
}

// Test the task has correct network usage rate when only having read bytes.
TEST_F(TaskGroupTest, NetworkBytesRead) {
  const int read_bytes = 1024;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER);
  fake_task.OnNetworkBytesRead(read_bytes);
  EXPECT_EQ(0, fake_task.network_usage_rate());
  EXPECT_EQ(read_bytes, fake_task.cumulative_network_usage());
  fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                    REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(read_bytes, fake_task.network_usage_rate());
  EXPECT_EQ(read_bytes, fake_task.cumulative_network_usage());
}

// Test the task has correct network usage rate when only having sent bytes.
TEST_F(TaskGroupTest, NetworkBytesSent) {
  const int sent_bytes = 1023;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER);
  fake_task.OnNetworkBytesSent(sent_bytes);
  EXPECT_EQ(0, fake_task.network_usage_rate());
  EXPECT_EQ(sent_bytes, fake_task.cumulative_network_usage());
  fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                    REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(sent_bytes, fake_task.network_usage_rate());
  EXPECT_EQ(sent_bytes, fake_task.cumulative_network_usage());
}

// Test the task has correct network usage rate when only having read bytes and
// having a non 1s refresh time.
TEST_F(TaskGroupTest, NetworkBytesRead2SecRefresh) {
  const int refresh_secs = 2;
  const int read_bytes = 1024 * refresh_secs;  // for integer division
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER);
  fake_task.OnNetworkBytesRead(read_bytes);
  EXPECT_EQ(0, fake_task.network_usage_rate());
  EXPECT_EQ(read_bytes, fake_task.cumulative_network_usage());
  fake_task.Refresh(base::TimeDelta::FromSeconds(refresh_secs),
                    REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(read_bytes / refresh_secs, fake_task.network_usage_rate());
  EXPECT_EQ(read_bytes, fake_task.cumulative_network_usage());
}

// Test the task has correct network usage rate when only having sent bytes and
// having a non 1s refresh time.
TEST_F(TaskGroupTest, NetworkBytesSent2SecRefresh) {
  const int refresh_secs = 2;
  const int sent_bytes = 1023 * refresh_secs;  // for integer division
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER);
  fake_task.OnNetworkBytesSent(sent_bytes);
  EXPECT_EQ(0, fake_task.network_usage_rate());
  EXPECT_EQ(sent_bytes, fake_task.cumulative_network_usage());
  fake_task.Refresh(base::TimeDelta::FromSeconds(refresh_secs),
                    REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(sent_bytes / refresh_secs, fake_task.network_usage_rate());
  EXPECT_EQ(sent_bytes, fake_task.cumulative_network_usage());
}

// Tests the task has correct usage on receiving and then sending bytes.
TEST_F(TaskGroupTest, NetworkBytesReadThenSent) {
  const int read_bytes = 124;
  const int sent_bytes = 1027;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER);
  fake_task.OnNetworkBytesRead(read_bytes);
  EXPECT_EQ(read_bytes, fake_task.cumulative_network_usage());
  fake_task.OnNetworkBytesSent(sent_bytes);
  fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                    REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(read_bytes + sent_bytes, fake_task.network_usage_rate());
  EXPECT_EQ(read_bytes + sent_bytes, fake_task.cumulative_network_usage());
}

// Tests the task has correct usage rate on sending and then receiving bytes.
TEST_F(TaskGroupTest, NetworkBytesSentThenRead) {
  const int read_bytes = 1025;
  const int sent_bytes = 10;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER);
  fake_task.OnNetworkBytesSent(sent_bytes);
  fake_task.OnNetworkBytesRead(read_bytes);
  fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                    REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(read_bytes + sent_bytes, fake_task.network_usage_rate());
}

// Tests that the network usage rate goes to 0 after reading bytes then a
// refresh with no traffic and that cumulative is still correct.
TEST_F(TaskGroupTest, NetworkBytesReadRefreshNone) {
  const int read_bytes = 1024;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER);
  fake_task.OnNetworkBytesRead(read_bytes);
  fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                    REFRESH_TYPE_NETWORK_USAGE);
  // Refresh to zero out the usage rate.
  fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                    REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, fake_task.network_usage_rate());
  EXPECT_EQ(read_bytes, fake_task.cumulative_network_usage());
}

// Tests that the network usage rate goes to 0 after sending bytes then a
// refresh with no traffic and that cumulative is still correct.
TEST_F(TaskGroupTest, NetworkBytesSentRefreshNone) {
  const int sent_bytes = 1024;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER);
  fake_task.OnNetworkBytesSent(sent_bytes);
  fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                    REFRESH_TYPE_NETWORK_USAGE);
  // Refresh to zero out the usage rate.
  fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                    REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, fake_task.network_usage_rate());
  EXPECT_EQ(sent_bytes, fake_task.cumulative_network_usage());
}

// Tests that the network usage rate goes to 0 after a refresh with no traffic
// and that cumulative is still correct.
TEST_F(TaskGroupTest, NetworkBytesTransferredRefreshNone) {
  const int read_bytes = 1024;
  const int sent_bytes = 1;
  const int number_of_cycles = 2;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER);
  for (int i = 0; i < number_of_cycles; i++) {
    fake_task.OnNetworkBytesRead(read_bytes);
    fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                      REFRESH_TYPE_NETWORK_USAGE);
    fake_task.OnNetworkBytesSent(sent_bytes);
    fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                      REFRESH_TYPE_NETWORK_USAGE);
  }
  // Refresh to zero out the usage rate.
  fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                    REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, fake_task.network_usage_rate());
  EXPECT_EQ((read_bytes + sent_bytes) * number_of_cycles,
            fake_task.cumulative_network_usage());
}

// Tests that 2 tasks in 1 task group that both read bytes have correct usage
// rates and correct cumulative network usage.
TEST_F(TaskGroupTest, NetworkBytesReadAsGroup) {
  const int read_bytes1 = 1024;
  const int read_bytes2 = 789;
  const int number_of_cycles = 2;
  FakeTask fake_task1(base::Process::Current().Pid(), Task::RENDERER);
  FakeTask fake_task2(base::Process::Current().Pid(), Task::RENDERER);

  task_group_.AddTask(&fake_task1);
  task_group_.AddTask(&fake_task2);

  for (int i = 0; i < number_of_cycles; i++) {
    fake_task1.OnNetworkBytesRead(read_bytes1);
    fake_task2.OnNetworkBytesRead(read_bytes2);
    task_group_.Refresh(gpu::VideoMemoryUsageStats(),
                        base::TimeDelta::FromSeconds(1),
                        REFRESH_TYPE_NETWORK_USAGE);
    EXPECT_EQ(read_bytes1 + read_bytes2,
              task_group_.per_process_network_usage_rate());
  }

  EXPECT_EQ((read_bytes1 + read_bytes2) * number_of_cycles,
            task_group_.cumulative_per_process_network_usage());
}

// Tests that the network usage rate does not get affected until a refresh is
// called and that the cumulative is as up to date as possible.
TEST_F(TaskGroupTest, NetworkBytesTransferredRefreshOutOfOrder) {
  const int read_bytes = 1024;
  const int sent_bytes = 1;
  const int number_of_cycles = 4;
  int number_of_bytes_transferred = 0;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER);
  for (int i = 0; i < number_of_cycles; i++) {
    fake_task.OnNetworkBytesRead(read_bytes * i);
    number_of_bytes_transferred += read_bytes * i;
    EXPECT_EQ(number_of_bytes_transferred,
              fake_task.cumulative_network_usage());
    fake_task.OnNetworkBytesSent(sent_bytes * i);
    number_of_bytes_transferred += sent_bytes * i;
    EXPECT_EQ(number_of_bytes_transferred,
              fake_task.cumulative_network_usage());
    if (i > 0) {
      EXPECT_EQ((read_bytes + sent_bytes) * (i - 1),
                fake_task.network_usage_rate());
    }
    fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                      REFRESH_TYPE_NETWORK_USAGE);
    EXPECT_EQ((read_bytes + sent_bytes) * i, fake_task.network_usage_rate());
  }
  // Refresh to zero out the usage rate.
  fake_task.Refresh(base::TimeDelta::FromSeconds(1),
                    REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, fake_task.network_usage_rate());
  EXPECT_EQ(number_of_bytes_transferred, fake_task.cumulative_network_usage());
}

// Tests that 2 tasks in 1 task group that both sent bytes have correct usage
// rates and correct cumulative network usage.
TEST_F(TaskGroupTest, NetworkBytesSentAsGroup) {
  const int sent_bytes1 = 1123;
  const int sent_bytes2 = 778;
  FakeTask fake_task1(base::Process::Current().Pid(), Task::RENDERER);
  FakeTask fake_task2(base::Process::Current().Pid(), Task::RENDERER);

  task_group_.AddTask(&fake_task1);
  task_group_.AddTask(&fake_task2);

  fake_task1.OnNetworkBytesSent(sent_bytes1);
  fake_task2.OnNetworkBytesSent(sent_bytes2);
  task_group_.Refresh(gpu::VideoMemoryUsageStats(),
                      base::TimeDelta::FromSeconds(1),
                      REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(sent_bytes1 + sent_bytes2,
            task_group_.per_process_network_usage_rate());

  fake_task1.OnNetworkBytesSent(sent_bytes1);
  fake_task2.OnNetworkBytesSent(sent_bytes2);
  task_group_.Refresh(gpu::VideoMemoryUsageStats(),
                      base::TimeDelta::FromSeconds(1),
                      REFRESH_TYPE_NETWORK_USAGE);

  EXPECT_EQ((sent_bytes1 + sent_bytes2) * 2,
            task_group_.cumulative_per_process_network_usage());
}

// Tests that 2 tasks in 1  task group that have one sending and one reading
// have correct usage rates for the group and correct cumulative network usage.
TEST_F(TaskGroupTest, NetworkBytesTransferredAsGroup) {
  const int sent_bytes = 1023;
  const int read_bytes = 678;
  const int number_of_cycles = 2;
  FakeTask fake_task1(base::Process::Current().Pid(), Task::RENDERER);
  FakeTask fake_task2(base::Process::Current().Pid(), Task::RENDERER);

  task_group_.AddTask(&fake_task1);
  task_group_.AddTask(&fake_task2);
  for (int i = 0; i < number_of_cycles; i++) {
    fake_task1.OnNetworkBytesSent(sent_bytes);
    fake_task2.OnNetworkBytesRead(read_bytes);
    task_group_.Refresh(gpu::VideoMemoryUsageStats(),
                        base::TimeDelta::FromSeconds(1),
                        REFRESH_TYPE_NETWORK_USAGE);
    EXPECT_EQ(sent_bytes + read_bytes,
              task_group_.per_process_network_usage_rate());
  }

  EXPECT_EQ((read_bytes + sent_bytes) * number_of_cycles,
            task_group_.cumulative_per_process_network_usage());
}

// Tests that after two tasks in a task group read bytes that a refresh will
// zero out network usage rate while maintaining the correct cumulative network
// usage.
TEST_F(TaskGroupTest, NetworkBytesReadAsGroupThenNone) {
  const int read_bytes1 = 1013;
  const int read_bytes2 = 679;
  const int number_of_cycles = 2;
  FakeTask fake_task1(base::Process::Current().Pid(), Task::RENDERER);
  FakeTask fake_task2(base::Process::Current().Pid(), Task::RENDERER);

  task_group_.AddTask(&fake_task1);
  task_group_.AddTask(&fake_task2);

  for (int i = 0; i < number_of_cycles; i++) {
    fake_task1.OnNetworkBytesRead(read_bytes1);
    fake_task2.OnNetworkBytesRead(read_bytes2);
    task_group_.Refresh(gpu::VideoMemoryUsageStats(),
                        base::TimeDelta::FromSeconds(1),
                        REFRESH_TYPE_NETWORK_USAGE);
    EXPECT_EQ(read_bytes1 + read_bytes2,
              task_group_.per_process_network_usage_rate());
  }
  task_group_.Refresh(gpu::VideoMemoryUsageStats(),
                      base::TimeDelta::FromSeconds(1),
                      REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, task_group_.per_process_network_usage_rate());
  EXPECT_EQ((read_bytes1 + read_bytes2) * number_of_cycles,
            task_group_.cumulative_per_process_network_usage());
}

// Tests that after two tasks in a task group send bytes that a refresh will
// zero out network usage rate while maintaining the correct cumulative network
// usage.
TEST_F(TaskGroupTest, NetworkBytesSentAsGroupThenNone) {
  const int sent_bytes1 = 1023;
  const int sent_bytes2 = 678;
  const int number_of_cycles = 2;
  FakeTask fake_task1(base::Process::Current().Pid(), Task::RENDERER);
  FakeTask fake_task2(base::Process::Current().Pid(), Task::RENDERER);

  task_group_.AddTask(&fake_task1);
  task_group_.AddTask(&fake_task2);

  for (int i = 0; i < number_of_cycles; i++) {
    fake_task1.OnNetworkBytesSent(sent_bytes1);
    fake_task2.OnNetworkBytesSent(sent_bytes2);
    task_group_.Refresh(gpu::VideoMemoryUsageStats(),
                        base::TimeDelta::FromSeconds(1),
                        REFRESH_TYPE_NETWORK_USAGE);
    EXPECT_EQ(sent_bytes1 + sent_bytes2,
              task_group_.per_process_network_usage_rate());
  }
  task_group_.Refresh(gpu::VideoMemoryUsageStats(),
                      base::TimeDelta::FromSeconds(1),
                      REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, task_group_.per_process_network_usage_rate());
  EXPECT_EQ((sent_bytes1 + sent_bytes2) * number_of_cycles,
            task_group_.cumulative_per_process_network_usage());
}

// Tests that after two tasks in a task group transferred bytes that a refresh
// will zero out network usage rate while maintaining the correct cumulative
// network usage.
TEST_F(TaskGroupTest, NetworkBytesTransferredAsGroupThenNone) {
  const int read_bytes = 321;
  const int sent_bytes = 987;
  const int number_of_cycles = 3;
  FakeTask fake_task1(base::Process::Current().Pid(), Task::RENDERER);
  FakeTask fake_task2(base::Process::Current().Pid(), Task::RENDERER);

  task_group_.AddTask(&fake_task1);
  task_group_.AddTask(&fake_task2);

  for (int i = 0; i < number_of_cycles; i++) {
    fake_task1.OnNetworkBytesRead(read_bytes);
    fake_task2.OnNetworkBytesSent(sent_bytes);
    task_group_.Refresh(gpu::VideoMemoryUsageStats(),
                        base::TimeDelta::FromSeconds(1),
                        REFRESH_TYPE_NETWORK_USAGE);
    EXPECT_EQ(read_bytes + sent_bytes,
              task_group_.per_process_network_usage_rate());
  }
  task_group_.Refresh(gpu::VideoMemoryUsageStats(),
                      base::TimeDelta::FromSeconds(1),
                      REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, task_group_.per_process_network_usage_rate());
  EXPECT_EQ((read_bytes + sent_bytes) * number_of_cycles,
            task_group_.cumulative_per_process_network_usage());
}

// Test the task can't be killed with a PID of base::kNullProcessId.
TEST_F(TaskGroupTest, TaskWithPidZero) {
  FakeTask fake_task(base::kNullProcessId, Task::RENDERER);
  EXPECT_FALSE(fake_task.IsKillable());
}

}  // namespace task_manager
