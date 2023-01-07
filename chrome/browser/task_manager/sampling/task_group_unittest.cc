// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/sampling/task_group.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/task_manager/sampling/shared_sampler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "gpu/ipc/common/memory_stats.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_manager {

namespace {

class FakeTask : public Task {
 public:
  FakeTask(base::ProcessId process_id, Type type, bool is_running_in_vm)
      : Task(std::u16string(), nullptr, base::kNullProcessHandle, process_id),
        type_(type),
        is_running_in_vm_(is_running_in_vm) {}

  FakeTask(const FakeTask&) = delete;
  FakeTask& operator=(const FakeTask&) = delete;

  Type GetType() const override { return type_; }

  int GetChildProcessUniqueID() const override { return 0; }

  const Task* GetParentTask() const override { return nullptr; }

  SessionID GetTabId() const override { return SessionID::InvalidValue(); }

  bool IsRunningInVM() const override { return is_running_in_vm_; }

 private:
  Type type_;
  bool is_running_in_vm_;
};

}  // namespace

class TaskGroupTest : public testing::Test {
 public:
  TaskGroupTest()
      : io_task_runner_(content::GetIOThreadTaskRunner({})),
        run_loop_(std::make_unique<base::RunLoop>()) {}

  TaskGroupTest(const TaskGroupTest&) = delete;
  TaskGroupTest& operator=(const TaskGroupTest&) = delete;

 protected:
  void OnBackgroundCalculationsDone() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    background_refresh_complete_ = true;
    run_loop_->QuitWhenIdle();
  }

  void CreateTaskGroup(bool is_running_in_vm) {
    task_group_ = std::make_unique<TaskGroup>(
        base::Process::Current().Handle(), base::Process::Current().Pid(),
        is_running_in_vm,
        base::BindRepeating(&TaskGroupTest::OnBackgroundCalculationsDone,
                            base::Unretained(this)),
        new SharedSampler(io_task_runner_),
#if BUILDFLAG(IS_CHROMEOS_ASH)
        /*crosapi_task_provider=*/nullptr,
#endif
        io_task_runner_);
    // Refresh() is only valid on non-empty TaskGroups, so add a fake Task.
    fake_task_ = std::make_unique<FakeTask>(base::Process::Current().Pid(),
                                            Task::UNKNOWN, is_running_in_vm);
    task_group_->AddTask(fake_task_.get());
  }

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<TaskGroup> task_group_;
  std::unique_ptr<FakeTask> fake_task_;
  bool background_refresh_complete_ = false;
};

// Verify that calling TaskGroup::Refresh() without specifying any fields to
// refresh trivially completes, without crashing or leaving things in a weird
// state.
TEST_F(TaskGroupTest, NullRefresh) {
  CreateTaskGroup(false);
  task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(), 0);
  EXPECT_TRUE(task_group_->AreBackgroundCalculationsDone());
  EXPECT_FALSE(background_refresh_complete_);
}

// Ensure that refreshing an empty TaskGroup causes a DCHECK (if enabled).
TEST_F(TaskGroupTest, RefreshZeroTasksDeathTest) {
  CreateTaskGroup(false);
  // Remove the fake Task from the group.
  task_group_->RemoveTask(fake_task_.get());

  EXPECT_DCHECK_DEATH(
      task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(), 0));
}

// Verify that Refresh() for a field which can be refreshed synchronously
// completes immediately, without leaving any background calculations pending.
TEST_F(TaskGroupTest, SyncRefresh) {
  CreateTaskGroup(false);
  task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(),
                       REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_TRUE(task_group_->AreBackgroundCalculationsDone());
  EXPECT_FALSE(background_refresh_complete_);
}

// Some fields are refreshed on a per-TaskGroup basis, but require asynchronous
// work (e.g. on another thread) to complete. Cpu is such a field, so verify
// that it is correctly reported as requiring background calculations.
TEST_F(TaskGroupTest, AsyncRefresh) {
  CreateTaskGroup(false);
  task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(),
                       REFRESH_TYPE_CPU);
  EXPECT_FALSE(task_group_->AreBackgroundCalculationsDone());

  ASSERT_FALSE(background_refresh_complete_);
  run_loop_->Run();

  EXPECT_TRUE(task_group_->AreBackgroundCalculationsDone());
  EXPECT_TRUE(background_refresh_complete_);
}

// Some fields are refreshed system-wide, via a SharedSampler, which completes
// asynchronously. Idle wakeups are reported via SharedSampler on some systems
// and via asynchronous refresh on others, so we just test that that field
// requires background calculations, similarly to the AsyncRefresh test above.
TEST_F(TaskGroupTest, SharedAsyncRefresh) {
  CreateTaskGroup(false);
  task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(),
                       REFRESH_TYPE_IDLE_WAKEUPS);
  EXPECT_FALSE(task_group_->AreBackgroundCalculationsDone());

  ASSERT_FALSE(background_refresh_complete_);
  run_loop_->Run();

  EXPECT_TRUE(background_refresh_complete_);

  EXPECT_TRUE(task_group_->AreBackgroundCalculationsDone());
}

// Ensure that if NaCl is enabled then calling Refresh with a NaCl Task active
// results in asynchronous completion. Also verifies that if NaCl is disabled
// then completion is synchronous.
TEST_F(TaskGroupTest, NaclRefreshWithTask) {
  CreateTaskGroup(false);
  FakeTask fake_task(base::Process::Current().Pid(), Task::NACL,
                     false /* is_running_in_vm */);
  task_group_->AddTask(&fake_task);

  task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(),
                       REFRESH_TYPE_NACL);
#if BUILDFLAG(ENABLE_NACL)
  EXPECT_FALSE(task_group_->AreBackgroundCalculationsDone());

  ASSERT_FALSE(background_refresh_complete_);
  run_loop_->Run();

  EXPECT_TRUE(background_refresh_complete_);
#endif  // BUILDFLAG(ENABLE_NACL)

  EXPECT_TRUE(task_group_->AreBackgroundCalculationsDone());
}

// Test the task has correct network usage rate when zero bytes read and sent.
TEST_F(TaskGroupTest, NetworkBytesSentReadZero) {
  CreateTaskGroup(false);
  const int zero_bytes = 0;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER,
                     false /* is_running_in_vm */);
  fake_task.OnNetworkBytesRead(zero_bytes);
  fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(zero_bytes, fake_task.GetNetworkUsageRate());
  fake_task.OnNetworkBytesSent(zero_bytes);
  fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(zero_bytes, fake_task.GetNetworkUsageRate());
}

// Test the task has correct network usage rate when only having read bytes.
TEST_F(TaskGroupTest, NetworkBytesRead) {
  CreateTaskGroup(false);
  const int read_bytes = 1024;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER,
                     false /* is_running_in_vm */);
  fake_task.OnNetworkBytesRead(read_bytes);
  EXPECT_EQ(0, fake_task.GetNetworkUsageRate());
  EXPECT_EQ(read_bytes, fake_task.GetCumulativeNetworkUsage());
  fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(read_bytes, fake_task.GetNetworkUsageRate());
  EXPECT_EQ(read_bytes, fake_task.GetCumulativeNetworkUsage());
}

// Test the task has correct network usage rate when only having sent bytes.
TEST_F(TaskGroupTest, NetworkBytesSent) {
  CreateTaskGroup(false);
  const int sent_bytes = 1023;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER,
                     false /* is_running_in_vm */);
  fake_task.OnNetworkBytesSent(sent_bytes);
  EXPECT_EQ(0, fake_task.GetNetworkUsageRate());
  EXPECT_EQ(sent_bytes, fake_task.GetCumulativeNetworkUsage());
  fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(sent_bytes, fake_task.GetNetworkUsageRate());
  EXPECT_EQ(sent_bytes, fake_task.GetCumulativeNetworkUsage());
}

// Test the task has correct network usage rate when only having read bytes and
// having a non 1s refresh time.
TEST_F(TaskGroupTest, NetworkBytesRead2SecRefresh) {
  CreateTaskGroup(false);
  const int refresh_secs = 2;
  const int read_bytes = 1024 * refresh_secs;  // for integer division
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER,
                     false /* is_running_in_vm */);
  fake_task.OnNetworkBytesRead(read_bytes);
  EXPECT_EQ(0, fake_task.GetNetworkUsageRate());
  EXPECT_EQ(read_bytes, fake_task.GetCumulativeNetworkUsage());
  fake_task.Refresh(base::Seconds(refresh_secs), REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(read_bytes / refresh_secs, fake_task.GetNetworkUsageRate());
  EXPECT_EQ(read_bytes, fake_task.GetCumulativeNetworkUsage());
}

// Test the task has correct network usage rate when only having sent bytes and
// having a non 1s refresh time.
TEST_F(TaskGroupTest, NetworkBytesSent2SecRefresh) {
  CreateTaskGroup(false);
  const int refresh_secs = 2;
  const int sent_bytes = 1023 * refresh_secs;  // for integer division
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER,
                     false /* is_running_in_vm */);
  fake_task.OnNetworkBytesSent(sent_bytes);
  EXPECT_EQ(0, fake_task.GetNetworkUsageRate());
  EXPECT_EQ(sent_bytes, fake_task.GetCumulativeNetworkUsage());
  fake_task.Refresh(base::Seconds(refresh_secs), REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(sent_bytes / refresh_secs, fake_task.GetNetworkUsageRate());
  EXPECT_EQ(sent_bytes, fake_task.GetCumulativeNetworkUsage());
}

// Tests the task has correct usage on receiving and then sending bytes.
TEST_F(TaskGroupTest, NetworkBytesReadThenSent) {
  CreateTaskGroup(false);
  const int read_bytes = 124;
  const int sent_bytes = 1027;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER,
                     false /* is_running_in_vm */);
  fake_task.OnNetworkBytesRead(read_bytes);
  EXPECT_EQ(read_bytes, fake_task.GetCumulativeNetworkUsage());
  fake_task.OnNetworkBytesSent(sent_bytes);
  fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(read_bytes + sent_bytes, fake_task.GetNetworkUsageRate());
  EXPECT_EQ(read_bytes + sent_bytes, fake_task.GetCumulativeNetworkUsage());
}

// Tests the task has correct usage rate on sending and then receiving bytes.
TEST_F(TaskGroupTest, NetworkBytesSentThenRead) {
  CreateTaskGroup(false);
  const int read_bytes = 1025;
  const int sent_bytes = 10;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER,
                     false /* is_running_in_vm */);
  fake_task.OnNetworkBytesSent(sent_bytes);
  fake_task.OnNetworkBytesRead(read_bytes);
  fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(read_bytes + sent_bytes, fake_task.GetNetworkUsageRate());
}

// Tests that the network usage rate goes to 0 after reading bytes then a
// refresh with no traffic and that cumulative is still correct.
TEST_F(TaskGroupTest, NetworkBytesReadRefreshNone) {
  CreateTaskGroup(false);
  const int read_bytes = 1024;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER,
                     false /* is_running_in_vm */);
  fake_task.OnNetworkBytesRead(read_bytes);
  fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  // Refresh to zero out the usage rate.
  fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, fake_task.GetNetworkUsageRate());
  EXPECT_EQ(read_bytes, fake_task.GetCumulativeNetworkUsage());
}

// Tests that the network usage rate goes to 0 after sending bytes then a
// refresh with no traffic and that cumulative is still correct.
TEST_F(TaskGroupTest, NetworkBytesSentRefreshNone) {
  CreateTaskGroup(false);
  const int sent_bytes = 1024;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER,
                     false /* is_running_in_vm */);
  fake_task.OnNetworkBytesSent(sent_bytes);
  fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  // Refresh to zero out the usage rate.
  fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, fake_task.GetNetworkUsageRate());
  EXPECT_EQ(sent_bytes, fake_task.GetCumulativeNetworkUsage());
}

// Tests that the network usage rate goes to 0 after a refresh with no traffic
// and that cumulative is still correct.
TEST_F(TaskGroupTest, NetworkBytesTransferredRefreshNone) {
  CreateTaskGroup(false);
  const int read_bytes = 1024;
  const int sent_bytes = 1;
  const int number_of_cycles = 2;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER,
                     false /* is_running_in_vm */);
  for (int i = 0; i < number_of_cycles; i++) {
    fake_task.OnNetworkBytesRead(read_bytes);
    fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
    fake_task.OnNetworkBytesSent(sent_bytes);
    fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  }
  // Refresh to zero out the usage rate.
  fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, fake_task.GetNetworkUsageRate());
  EXPECT_EQ((read_bytes + sent_bytes) * number_of_cycles,
            fake_task.GetCumulativeNetworkUsage());
}

// Tests that 2 tasks in 1 task group that both read bytes have correct usage
// rates and correct cumulative network usage.
TEST_F(TaskGroupTest, NetworkBytesReadAsGroup) {
  CreateTaskGroup(false);
  const int read_bytes1 = 1024;
  const int read_bytes2 = 789;
  const int number_of_cycles = 2;
  FakeTask fake_task1(base::Process::Current().Pid(), Task::RENDERER,
                      false /* is_running_in_vm */);
  FakeTask fake_task2(base::Process::Current().Pid(), Task::RENDERER,
                      false /* is_running_in_vm */);

  task_group_->AddTask(&fake_task1);
  task_group_->AddTask(&fake_task2);

  for (int i = 0; i < number_of_cycles; i++) {
    fake_task1.OnNetworkBytesRead(read_bytes1);
    fake_task2.OnNetworkBytesRead(read_bytes2);
    task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::Seconds(1),
                         REFRESH_TYPE_NETWORK_USAGE);
    EXPECT_EQ(read_bytes1 + read_bytes2,
              task_group_->per_process_network_usage_rate());
  }

  EXPECT_EQ((read_bytes1 + read_bytes2) * number_of_cycles,
            task_group_->cumulative_per_process_network_usage());
}

// Tests that the network usage rate does not get affected until a refresh is
// called and that the cumulative is as up to date as possible.
TEST_F(TaskGroupTest, NetworkBytesTransferredRefreshOutOfOrder) {
  CreateTaskGroup(false);
  const int read_bytes = 1024;
  const int sent_bytes = 1;
  const int number_of_cycles = 4;
  int number_of_bytes_transferred = 0;
  FakeTask fake_task(base::Process::Current().Pid(), Task::RENDERER,
                     false /* is_running_in_vm */);
  for (int i = 0; i < number_of_cycles; i++) {
    fake_task.OnNetworkBytesRead(read_bytes * i);
    number_of_bytes_transferred += read_bytes * i;
    EXPECT_EQ(number_of_bytes_transferred,
              fake_task.GetCumulativeNetworkUsage());
    fake_task.OnNetworkBytesSent(sent_bytes * i);
    number_of_bytes_transferred += sent_bytes * i;
    EXPECT_EQ(number_of_bytes_transferred,
              fake_task.GetCumulativeNetworkUsage());
    if (i > 0) {
      EXPECT_EQ((read_bytes + sent_bytes) * (i - 1),
                fake_task.GetNetworkUsageRate());
    }
    fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
    EXPECT_EQ((read_bytes + sent_bytes) * i, fake_task.GetNetworkUsageRate());
  }
  // Refresh to zero out the usage rate.
  fake_task.Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, fake_task.GetNetworkUsageRate());
  EXPECT_EQ(number_of_bytes_transferred, fake_task.GetCumulativeNetworkUsage());
}

// Tests that 2 tasks in 1 task group that both sent bytes have correct usage
// rates and correct cumulative network usage.
TEST_F(TaskGroupTest, NetworkBytesSentAsGroup) {
  CreateTaskGroup(false);
  const int sent_bytes1 = 1123;
  const int sent_bytes2 = 778;
  FakeTask fake_task1(base::Process::Current().Pid(), Task::RENDERER,
                      false /* is_running_in_vm */);
  FakeTask fake_task2(base::Process::Current().Pid(), Task::RENDERER,
                      false /* is_running_in_vm */);

  task_group_->AddTask(&fake_task1);
  task_group_->AddTask(&fake_task2);

  fake_task1.OnNetworkBytesSent(sent_bytes1);
  fake_task2.OnNetworkBytesSent(sent_bytes2);
  task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::Seconds(1),
                       REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(sent_bytes1 + sent_bytes2,
            task_group_->per_process_network_usage_rate());

  fake_task1.OnNetworkBytesSent(sent_bytes1);
  fake_task2.OnNetworkBytesSent(sent_bytes2);
  task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::Seconds(1),
                       REFRESH_TYPE_NETWORK_USAGE);

  EXPECT_EQ((sent_bytes1 + sent_bytes2) * 2,
            task_group_->cumulative_per_process_network_usage());
}

// Tests that 2 tasks in 1  task group that have one sending and one reading
// have correct usage rates for the group and correct cumulative network usage.
TEST_F(TaskGroupTest, NetworkBytesTransferredAsGroup) {
  CreateTaskGroup(false);
  const int sent_bytes = 1023;
  const int read_bytes = 678;
  const int number_of_cycles = 2;
  FakeTask fake_task1(base::Process::Current().Pid(), Task::RENDERER,
                      false /* is_running_in_vm */);
  FakeTask fake_task2(base::Process::Current().Pid(), Task::RENDERER,
                      false /* is_running_in_vm */);

  task_group_->AddTask(&fake_task1);
  task_group_->AddTask(&fake_task2);
  for (int i = 0; i < number_of_cycles; i++) {
    fake_task1.OnNetworkBytesSent(sent_bytes);
    fake_task2.OnNetworkBytesRead(read_bytes);
    task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::Seconds(1),
                         REFRESH_TYPE_NETWORK_USAGE);
    EXPECT_EQ(sent_bytes + read_bytes,
              task_group_->per_process_network_usage_rate());
  }

  EXPECT_EQ((read_bytes + sent_bytes) * number_of_cycles,
            task_group_->cumulative_per_process_network_usage());
}

// Tests that after two tasks in a task group read bytes that a refresh will
// zero out network usage rate while maintaining the correct cumulative network
// usage.
TEST_F(TaskGroupTest, NetworkBytesReadAsGroupThenNone) {
  CreateTaskGroup(false);
  const int read_bytes1 = 1013;
  const int read_bytes2 = 679;
  const int number_of_cycles = 2;
  FakeTask fake_task1(base::Process::Current().Pid(), Task::RENDERER,
                      false /* is_running_in_vm */);
  FakeTask fake_task2(base::Process::Current().Pid(), Task::RENDERER,
                      false /* is_running_in_vm */);

  task_group_->AddTask(&fake_task1);
  task_group_->AddTask(&fake_task2);

  for (int i = 0; i < number_of_cycles; i++) {
    fake_task1.OnNetworkBytesRead(read_bytes1);
    fake_task2.OnNetworkBytesRead(read_bytes2);
    task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::Seconds(1),
                         REFRESH_TYPE_NETWORK_USAGE);
    EXPECT_EQ(read_bytes1 + read_bytes2,
              task_group_->per_process_network_usage_rate());
  }
  task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::Seconds(1),
                       REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, task_group_->per_process_network_usage_rate());
  EXPECT_EQ((read_bytes1 + read_bytes2) * number_of_cycles,
            task_group_->cumulative_per_process_network_usage());
}

// Tests that after two tasks in a task group send bytes that a refresh will
// zero out network usage rate while maintaining the correct cumulative network
// usage.
TEST_F(TaskGroupTest, NetworkBytesSentAsGroupThenNone) {
  CreateTaskGroup(false);
  const int sent_bytes1 = 1023;
  const int sent_bytes2 = 678;
  const int number_of_cycles = 2;
  FakeTask fake_task1(base::Process::Current().Pid(), Task::RENDERER,
                      false /* is_running_in_vm */);
  FakeTask fake_task2(base::Process::Current().Pid(), Task::RENDERER,
                      false /* is_running_in_vm */);

  task_group_->AddTask(&fake_task1);
  task_group_->AddTask(&fake_task2);

  for (int i = 0; i < number_of_cycles; i++) {
    fake_task1.OnNetworkBytesSent(sent_bytes1);
    fake_task2.OnNetworkBytesSent(sent_bytes2);
    task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::Seconds(1),
                         REFRESH_TYPE_NETWORK_USAGE);
    EXPECT_EQ(sent_bytes1 + sent_bytes2,
              task_group_->per_process_network_usage_rate());
  }
  task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::Seconds(1),
                       REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, task_group_->per_process_network_usage_rate());
  EXPECT_EQ((sent_bytes1 + sent_bytes2) * number_of_cycles,
            task_group_->cumulative_per_process_network_usage());
}

// Tests that after two tasks in a task group transferred bytes that a refresh
// will zero out network usage rate while maintaining the correct cumulative
// network usage.
TEST_F(TaskGroupTest, NetworkBytesTransferredAsGroupThenNone) {
  CreateTaskGroup(false);
  const int read_bytes = 321;
  const int sent_bytes = 987;
  const int number_of_cycles = 3;
  FakeTask fake_task1(base::Process::Current().Pid(), Task::RENDERER,
                      false /* is_running_in_vm */);
  FakeTask fake_task2(base::Process::Current().Pid(), Task::RENDERER,
                      false /* is_running_in_vm */);

  task_group_->AddTask(&fake_task1);
  task_group_->AddTask(&fake_task2);

  for (int i = 0; i < number_of_cycles; i++) {
    fake_task1.OnNetworkBytesRead(read_bytes);
    fake_task2.OnNetworkBytesSent(sent_bytes);
    task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::Seconds(1),
                         REFRESH_TYPE_NETWORK_USAGE);
    EXPECT_EQ(read_bytes + sent_bytes,
              task_group_->per_process_network_usage_rate());
  }
  task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::Seconds(1),
                       REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(0, task_group_->per_process_network_usage_rate());
  EXPECT_EQ((read_bytes + sent_bytes) * number_of_cycles,
            task_group_->cumulative_per_process_network_usage());
}

// Test the task can't be killed with a PID of base::kNullProcessId.
TEST_F(TaskGroupTest, TaskWithPidZero) {
  CreateTaskGroup(false);
  FakeTask fake_task(base::kNullProcessId, Task::RENDERER,
                     false /* is_running_in_vm */);
  EXPECT_FALSE(fake_task.IsKillable());
}

// Verify that calling TaskGroup::Refresh() on a VM task group with no supported
// refresh flags trivially completes.
TEST_F(TaskGroupTest, UnsupportedVMRefreshFlags) {
  CreateTaskGroup(true);
  task_group_->Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(),
                       task_manager::kUnsupportedVMRefreshFlags);
  EXPECT_TRUE(task_group_->AreBackgroundCalculationsDone());
  EXPECT_FALSE(background_refresh_complete_);
}

}  // namespace task_manager
