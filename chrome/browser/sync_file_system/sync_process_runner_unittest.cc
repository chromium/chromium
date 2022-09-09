// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_process_runner.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/containers/queue.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {

namespace {

class FakeClient : public SyncProcessRunner::Client {
 public:
  FakeClient() : service_state_(SYNC_SERVICE_RUNNING) {}

  FakeClient(const FakeClient&) = delete;
  FakeClient& operator=(const FakeClient&) = delete;

  ~FakeClient() override {}

  SyncServiceState GetSyncServiceState() override { return service_state_; }

  SyncFileSystemService* GetSyncService() override { return nullptr; }

  void set_service_state(SyncServiceState service_state) {
    service_state_ = service_state;
  }

 private:
  SyncServiceState service_state_;
};

class FakeTimerHelper : public SyncProcessRunner::TimerHelper {
 public:
  FakeTimerHelper() {}

  FakeTimerHelper(const FakeTimerHelper&) = delete;
  FakeTimerHelper& operator=(const FakeTimerHelper&) = delete;

  ~FakeTimerHelper() override {}

  bool IsRunning() override { return !timer_task_.is_null(); }

  void Start(const base::Location& from_here,
             const base::TimeDelta& delay,
             base::OnceClosure closure) override {
    scheduled_time_ = current_time_ + delay;
    timer_task_ = std::move(closure);
  }

  base::TimeTicks Now() const override { return current_time_; }

  void SetCurrentTime(const base::TimeTicks& current_time) {
    current_time_ = current_time;
    if (current_time_ < scheduled_time_ || timer_task_.is_null())
      return;

    std::move(timer_task_).Run();
  }

  void AdvanceToScheduledTime() {
    SetCurrentTime(scheduled_time_);
  }

  int64_t GetCurrentDelay() {
    EXPECT_FALSE(timer_task_.is_null());
    return (scheduled_time_ - current_time_).InMilliseconds();
  }

 private:
  base::TimeTicks current_time_;
  base::TimeTicks scheduled_time_;
  base::OnceClosure timer_task_;
};

class FakeSyncProcessRunner : public SyncProcessRunner {
 public:
  FakeSyncProcessRunner(SyncProcessRunner::Client* client,
                        std::unique_ptr<TimerHelper> timer_helper,
                        size_t max_parallel_task)
      : SyncProcessRunner("FakeSyncProcess",
                          client,
                          std::move(timer_helper),
                          max_parallel_task),
        max_parallel_task_(max_parallel_task) {}

  void StartSync(SyncStatusCallback callback) override {
    EXPECT_LT(running_tasks_.size(), max_parallel_task_);
    running_tasks_.push(std::move(callback));
  }

  FakeSyncProcessRunner(const FakeSyncProcessRunner&) = delete;
  FakeSyncProcessRunner& operator=(const FakeSyncProcessRunner&) = delete;

  ~FakeSyncProcessRunner() override {}

  void UpdateChanges(int num_changes) {
    OnChangesUpdated(num_changes);
  }

  void CompleteTask(SyncStatusCode status) {
    ASSERT_FALSE(running_tasks_.empty());
    SyncStatusCallback task = std::move(running_tasks_.front());
    running_tasks_.pop();
    std::move(task).Run(status);
  }

  bool HasRunningTask() const {
    return !running_tasks_.empty();
  }

 private:
  size_t max_parallel_task_;
  base::queue<SyncStatusCallback> running_tasks_;
};

}  // namespace

TEST(SyncProcessRunnerTest, SingleTaskBasicTest) {
  FakeClient fake_client;
  FakeTimerHelper* fake_timer = new FakeTimerHelper();
  FakeSyncProcessRunner fake_runner(
      &fake_client, std::unique_ptr<SyncProcessRunner::TimerHelper>(fake_timer),
      1 /* max_parallel_task */);

  base::TimeTicks base_time = base::TimeTicks::Now();
  fake_timer->SetCurrentTime(base_time);

  // SyncProcessRunner is expected not to run a task  initially.
  EXPECT_FALSE(fake_timer->IsRunning());

  // As soon as SyncProcessRunner gets a new update, it should start running
  // the timer to run a synchronization task.
  fake_runner.UpdateChanges(100);
  EXPECT_TRUE(fake_timer->IsRunning());
  EXPECT_EQ(SyncProcessRunner::kSyncDelayFastInMilliseconds,
            fake_timer->GetCurrentDelay());

  // When the time has come, the timer should fire the scheduled task.
  fake_timer->AdvanceToScheduledTime();
  EXPECT_FALSE(fake_timer->IsRunning());
  EXPECT_TRUE(fake_runner.HasRunningTask());

  // Successful completion of the task fires next synchronization task.
  fake_runner.CompleteTask(SYNC_STATUS_OK);
  EXPECT_TRUE(fake_timer->IsRunning());
  EXPECT_FALSE(fake_runner.HasRunningTask());
  EXPECT_EQ(SyncProcessRunner::kSyncDelayFastInMilliseconds,
            fake_timer->GetCurrentDelay());

  // Turn |service_state| to TEMPORARY_UNAVAILABLE and let the task fail.
  // |fake_runner| should schedule following tasks with longer delay.
  fake_timer->AdvanceToScheduledTime();
  fake_client.set_service_state(SYNC_SERVICE_TEMPORARY_UNAVAILABLE);
  fake_runner.CompleteTask(SYNC_STATUS_FAILED);
  EXPECT_EQ(SyncProcessRunner::kSyncDelaySlowInMilliseconds,
            fake_timer->GetCurrentDelay());

  // Repeated failure makes the task delay back off.
  fake_timer->AdvanceToScheduledTime();
  fake_runner.CompleteTask(SYNC_STATUS_FAILED);
  EXPECT_EQ(2 * SyncProcessRunner::kSyncDelaySlowInMilliseconds,
            fake_timer->GetCurrentDelay());

  // After |service_state| gets back to normal state, SyncProcessRunner should
  // restart rapid task invocation.
  fake_client.set_service_state(SYNC_SERVICE_RUNNING);
  fake_timer->AdvanceToScheduledTime();
  fake_runner.CompleteTask(SYNC_STATUS_OK);
  EXPECT_EQ(SyncProcessRunner::kSyncDelayFastInMilliseconds,
            fake_timer->GetCurrentDelay());

  // There's no item to sync anymore, SyncProcessRunner should schedules the
  // next with the longest delay.
  fake_runner.UpdateChanges(0);
  fake_timer->AdvanceToScheduledTime();
  fake_runner.CompleteTask(SYNC_STATUS_OK);
  EXPECT_EQ(SyncProcessRunner::kSyncDelayMaxInMilliseconds,
            fake_timer->GetCurrentDelay());

  // Schedule the next with the longest delay if the client is persistently
  // unavailable.
  fake_client.set_service_state(SYNC_SERVICE_AUTHENTICATION_REQUIRED);
  fake_runner.UpdateChanges(100);
  EXPECT_EQ(SyncProcessRunner::kSyncDelayMaxInMilliseconds,
            fake_timer->GetCurrentDelay());
}

TEST(SyncProcessRunnerTest, MultiTaskBasicTest) {
  FakeClient fake_client;
  FakeTimerHelper* fake_timer = new FakeTimerHelper();
  FakeSyncProcessRunner fake_runner(
      &fake_client, std::unique_ptr<SyncProcessRunner::TimerHelper>(fake_timer),
      2 /* max_parallel_task */);

  base::TimeTicks base_time = base::TimeTicks::Now();
  fake_timer->SetCurrentTime(base_time);

  EXPECT_FALSE(fake_timer->IsRunning());

  fake_runner.UpdateChanges(100);
  EXPECT_TRUE(fake_timer->IsRunning());
  EXPECT_EQ(SyncProcessRunner::kSyncDelayFastInMilliseconds,
            fake_timer->GetCurrentDelay());

  // Even after a task starts running, SyncProcessRunner should schedule next
  // task until the number of running task reachs the limit.
  fake_timer->AdvanceToScheduledTime();
  EXPECT_TRUE(fake_timer->IsRunning());
  EXPECT_TRUE(fake_runner.HasRunningTask());
  EXPECT_EQ(SyncProcessRunner::kSyncDelayFastInMilliseconds,
            fake_timer->GetCurrentDelay());

  // After the second task starts running, SyncProcessRunner should stop
  // scheduling a task.
  fake_timer->AdvanceToScheduledTime();
  EXPECT_FALSE(fake_timer->IsRunning());
  EXPECT_TRUE(fake_runner.HasRunningTask());

  fake_runner.CompleteTask(SYNC_STATUS_OK);
  EXPECT_TRUE(fake_timer->IsRunning());
  EXPECT_TRUE(fake_runner.HasRunningTask());
  fake_runner.CompleteTask(SYNC_STATUS_OK);
  EXPECT_TRUE(fake_timer->IsRunning());
  EXPECT_FALSE(fake_runner.HasRunningTask());

  // Turn |service_state| to TEMPORARY_UNAVAILABLE and let the task fail.
  // |fake_runner| should schedule following tasks with longer delay.
  fake_timer->AdvanceToScheduledTime();
  fake_timer->AdvanceToScheduledTime();
  fake_client.set_service_state(SYNC_SERVICE_TEMPORARY_UNAVAILABLE);
  fake_runner.CompleteTask(SYNC_STATUS_FAILED);
  EXPECT_EQ(SyncProcessRunner::kSyncDelaySlowInMilliseconds,
            fake_timer->GetCurrentDelay());

  // Consecutive error reports shouldn't extend delay immediately.
  fake_runner.CompleteTask(SYNC_STATUS_FAILED);
  EXPECT_EQ(SyncProcessRunner::kSyncDelaySlowInMilliseconds,
            fake_timer->GetCurrentDelay());

  // The next task will run after throttle period is over.
  // And its failure should extend the throttle period by twice.
  fake_timer->AdvanceToScheduledTime();
  EXPECT_EQ(SyncProcessRunner::kSyncDelaySlowInMilliseconds,
            fake_timer->GetCurrentDelay());
  fake_runner.CompleteTask(SYNC_STATUS_FAILED);
  EXPECT_EQ(2 * SyncProcessRunner::kSyncDelaySlowInMilliseconds,
            fake_timer->GetCurrentDelay());

  // Next successful task should clear the throttling.
  fake_timer->AdvanceToScheduledTime();
  fake_client.set_service_state(SYNC_SERVICE_RUNNING);
  fake_runner.CompleteTask(SYNC_STATUS_OK);
  EXPECT_EQ(SyncProcessRunner::kSyncDelayFastInMilliseconds,
            fake_timer->GetCurrentDelay());

  // Then, following failing task should not extend throttling period.
  fake_timer->AdvanceToScheduledTime();
  fake_client.set_service_state(SYNC_SERVICE_TEMPORARY_UNAVAILABLE);
  fake_runner.CompleteTask(SYNC_STATUS_FAILED);
  EXPECT_EQ(SyncProcessRunner::kSyncDelaySlowInMilliseconds,
            fake_timer->GetCurrentDelay());
}

}  // namespace sync_file_system
