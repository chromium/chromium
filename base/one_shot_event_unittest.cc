// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/one_shot_event.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

void Increment(int* i) {
  ++*i;
}

// |*did_delete_instance| will be set to true upon its destruction.
class RefCountedClass : public base::RefCounted<RefCountedClass> {
 public:
  explicit RefCountedClass(bool* did_delete_instance)
      : did_delete_instance_(did_delete_instance) {
    DCHECK(!*did_delete_instance_);
  }

  void PerformTask() { did_perform_task_ = true; }
  bool did_perform_task() const { return did_perform_task_; }

 private:
  friend class base::RefCounted<RefCountedClass>;

  ~RefCountedClass() { *did_delete_instance_ = true; }

  bool* const did_delete_instance_;  // Not owned.

  bool did_perform_task_ = false;

  DISALLOW_COPY_AND_ASSIGN(RefCountedClass);
};

TEST(OneShotEventTest, RecordsSignal) {
  OneShotEvent event;
  EXPECT_FALSE(event.is_signaled());
  event.Signal();
  EXPECT_TRUE(event.is_signaled());
}

TEST(OneShotEventTest, CallsQueueAsDistinctTask) {
  OneShotEvent event;
  scoped_refptr<base::TestSimpleTaskRunner> runner(
      new base::TestSimpleTaskRunner);
  int i = 0;
  event.Post(FROM_HERE, base::BindOnce(&Increment, &i), runner);
  event.Post(FROM_HERE, base::BindOnce(&Increment, &i), runner);
  EXPECT_EQ(0U, runner->NumPendingTasks());
  event.Signal();

  auto pending_tasks = runner->TakePendingTasks();
  ASSERT_EQ(2U, pending_tasks.size());
  EXPECT_NE(pending_tasks[0].location.line_number(),
            pending_tasks[1].location.line_number())
      << "Make sure FROM_HERE is propagated.";
}

TEST(OneShotEventTest, CallsQueue) {
  OneShotEvent event;
  scoped_refptr<base::TestSimpleTaskRunner> runner(
      new base::TestSimpleTaskRunner);
  int i = 0;
  event.Post(FROM_HERE, base::BindOnce(&Increment, &i), runner);
  event.Post(FROM_HERE, base::BindOnce(&Increment, &i), runner);
  EXPECT_EQ(0U, runner->NumPendingTasks());
  event.Signal();
  ASSERT_EQ(2U, runner->NumPendingTasks());

  EXPECT_EQ(0, i);
  runner->RunPendingTasks();
  EXPECT_EQ(2, i);
}

TEST(OneShotEventTest, CallsAfterSignalDontRunInline) {
  OneShotEvent event;
  scoped_refptr<base::TestSimpleTaskRunner> runner(
      new base::TestSimpleTaskRunner);
  int i = 0;

  event.Signal();
  event.Post(FROM_HERE, base::BindOnce(&Increment, &i), runner);
  EXPECT_EQ(1U, runner->NumPendingTasks());
  EXPECT_EQ(0, i);
  runner->RunPendingTasks();
  EXPECT_EQ(1, i);
}

TEST(OneShotEventTest, PostDefaultsToCurrentMessageLoop) {
  OneShotEvent event;
  scoped_refptr<base::TestSimpleTaskRunner> runner(
      new base::TestSimpleTaskRunner);
  base::test::SingleThreadTaskEnvironment task_environment;
  int runner_i = 0;
  int loop_i = 0;

  event.Post(FROM_HERE, base::BindOnce(&Increment, &runner_i), runner);
  event.Post(FROM_HERE, base::BindOnce(&Increment, &loop_i));
  event.Signal();
  EXPECT_EQ(1U, runner->NumPendingTasks());
  EXPECT_EQ(0, runner_i);
  runner->RunPendingTasks();
  EXPECT_EQ(1, runner_i);
  EXPECT_EQ(0, loop_i);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, loop_i);
}

void CheckSignaledAndPostIncrement(
    OneShotEvent* event,
    const scoped_refptr<base::SingleThreadTaskRunner>& runner,
    int* i) {
  EXPECT_TRUE(event->is_signaled());
  event->Post(FROM_HERE, base::BindOnce(&Increment, i), runner);
}

TEST(OneShotEventTest, IsSignaledAndPostsFromCallbackWork) {
  OneShotEvent event;
  scoped_refptr<base::TestSimpleTaskRunner> runner(
      new base::TestSimpleTaskRunner);
  int i = 0;

  event.Post(FROM_HERE,
             base::BindOnce(&CheckSignaledAndPostIncrement, &event, runner, &i),
             runner);
  EXPECT_EQ(0, i);
  event.Signal();

  // CheckSignaledAndPostIncrement is queued on |runner|.
  EXPECT_EQ(1U, runner->NumPendingTasks());
  EXPECT_EQ(0, i);
  runner->RunPendingTasks();
  // Increment is queued on |runner|.
  EXPECT_EQ(1U, runner->NumPendingTasks());
  EXPECT_EQ(0, i);
  runner->RunPendingTasks();
  // Increment has run.
  EXPECT_EQ(0U, runner->NumPendingTasks());
  EXPECT_EQ(1, i);
}

// Tests that OneShotEvent does not keep references to tasks once OneShotEvent
// Signal()s.
TEST(OneShotEventTest, DropsCallbackRefUponSignalled) {
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  bool did_delete_instance = false;
  OneShotEvent event;

  {
    auto ref_counted_class =
        base::MakeRefCounted<RefCountedClass>(&did_delete_instance);
    event.Post(FROM_HERE,
               base::BindOnce(&RefCountedClass::PerformTask, ref_counted_class),
               runner);
    event.Signal();
    runner->RunPendingTasks();
    EXPECT_TRUE(ref_counted_class->did_perform_task());
  }

  // Once OneShotEvent doesn't have any queued events, it should have dropped
  // all the references to the callbacks it received through Post().
  EXPECT_TRUE(did_delete_instance);
}

}  // namespace base
