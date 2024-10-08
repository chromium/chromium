// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/java_handler_thread.h"

#include "base/synchronization/waitable_event.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/task_observer.h"
#include "base/test/android/java_handler_thread_helpers.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class JavaHandlerThreadForTest : public android::JavaHandlerThread {
 public:
  explicit JavaHandlerThreadForTest(
      const char* name,
      base::ThreadType thread_type = base::ThreadType::kDefault)
      : android::JavaHandlerThread(name, thread_type) {}

  using android::JavaHandlerThread::state;
  using android::JavaHandlerThread::State;
};

class DummyTaskObserver : public TaskObserver {
 public:
  explicit DummyTaskObserver(int num_tasks)
      : num_tasks_started_(0), num_tasks_processed_(0), num_tasks_(num_tasks) {}

  DummyTaskObserver(int num_tasks, int num_tasks_started)
      : num_tasks_started_(num_tasks_started),
        num_tasks_processed_(0),
        num_tasks_(num_tasks) {}

  DummyTaskObserver(const DummyTaskObserver&) = delete;
  DummyTaskObserver& operator=(const DummyTaskObserver&) = delete;

  ~DummyTaskObserver() override = default;

  void WillProcessTask(const PendingTask& /* pending_task */,
                       bool /* was_blocked_or_low_priority */) override {
    num_tasks_started_++;
    EXPECT_LE(num_tasks_started_, num_tasks_);
    EXPECT_EQ(num_tasks_started_, num_tasks_processed_ + 1);
  }

  void DidProcessTask(const PendingTask& pending_task) override {
    num_tasks_processed_++;
    EXPECT_LE(num_tasks_started_, num_tasks_);
    EXPECT_EQ(num_tasks_started_, num_tasks_processed_);
  }

  int num_tasks_started() const { return num_tasks_started_; }
  int num_tasks_processed() const { return num_tasks_processed_; }

 private:
  int num_tasks_started_;
  int num_tasks_processed_;
  const int num_tasks_;
};

void PostNTasks(int posts_remaining) {
  if (posts_remaining > 1) {
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindOnce(&PostNTasks, posts_remaining - 1));
  }
}

}  // namespace

class JavaHandlerThreadTest : public ::testing::Test {};

void RunTest_AbortDontRunMoreTasks(bool delayed, bool init_java_first) {
  WaitableEvent test_done_event;
  std::unique_ptr<android::JavaHandlerThread> java_thread;
  if (init_java_first) {
    java_thread = android::JavaHandlerThreadHelpers::CreateJavaFirst();
  } else {
    java_thread = std::make_unique<android::JavaHandlerThread>(
        "JavaHandlerThreadForTesting from AbortDontRunMoreTasks");
  }
  java_thread->Start();
  java_thread->ListenForUncaughtExceptionsForTesting();

  auto target =
      BindOnce(&android::JavaHandlerThreadHelpers::ThrowExceptionAndAbort,
               &test_done_event);
  if (delayed) {
    java_thread->task_runner()->PostDelayedTask(FROM_HERE, std::move(target),
                                                Milliseconds(10));
  } else {
    java_thread->task_runner()->PostTask(FROM_HERE, std::move(target));
    java_thread->task_runner()->PostTask(FROM_HERE,
                                         MakeExpectedNotRunClosure(FROM_HERE));
  }
  test_done_event.Wait();
  java_thread->Stop();
  android::ScopedJavaLocalRef<jthrowable> exception =
      java_thread->GetUncaughtExceptionIfAny();
  ASSERT_TRUE(
      android::JavaHandlerThreadHelpers::IsExceptionTestException(exception));
}

TEST_F(JavaHandlerThreadTest, JavaExceptionAbort) {
  constexpr bool delayed = false;
  constexpr bool init_java_first = false;
  RunTest_AbortDontRunMoreTasks(delayed, init_java_first);
}

TEST_F(JavaHandlerThreadTest, DelayedJavaExceptionAbort) {
  constexpr bool delayed = true;
  constexpr bool init_java_first = false;
  RunTest_AbortDontRunMoreTasks(delayed, init_java_first);
}

TEST_F(JavaHandlerThreadTest, JavaExceptionAbortInitJavaFirst) {
  constexpr bool delayed = false;
  constexpr bool init_java_first = true;
  RunTest_AbortDontRunMoreTasks(delayed, init_java_first);
}

TEST_F(JavaHandlerThreadTest, RunTasksWhileShuttingDownJavaThread) {
  const int kNumPosts = 6;
  DummyTaskObserver observer(kNumPosts, 1);

  auto java_thread = std::make_unique<JavaHandlerThreadForTest>("test");
  java_thread->Start();

  sequence_manager::internal::SequenceManagerImpl* sequence_manager =
      static_cast<sequence_manager::internal::SequenceManagerImpl*>(
          java_thread->state()->sequence_manager.get());

  java_thread->task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        sequence_manager->AddTaskObserver(&observer);
        SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE, MakeExpectedNotRunClosure(FROM_HERE), Days(1));
        java_thread->StopSequenceManagerForTesting();
        PostNTasks(kNumPosts);
      }));

  java_thread->JoinForTesting();
  java_thread.reset();

  EXPECT_EQ(kNumPosts, observer.num_tasks_started());
  EXPECT_EQ(kNumPosts, observer.num_tasks_processed());
}

}  // namespace base
