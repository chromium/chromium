// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/pre_freeze_background_memory_trimmer.h"

#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

static int s_counter = 0;

void ResetGlobalCounter() {
  s_counter = 0;
}

void IncGlobalCounter() {
  s_counter++;
}

void PostDelayedIncGlobal() {
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(10));
}

}  // namespace

class PreFreezeBackgroundMemoryTrimmerTest : public testing::Test {
 public:
  PreFreezeBackgroundMemoryTrimmerTest() {
    fl_.InitAndEnableFeature(kOnPreFreezeMemoryTrim);
  }

  void SetUp() override {
    PreFreezeBackgroundMemoryTrimmer::SetIsRespectingModernTrimForTesting(true);
    ResetGlobalCounter();
  }

 protected:
  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  test::ScopedFeatureList fl_;
};

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, PostDelayedTaskSimple) {
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(30));

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            1u);

  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            0u);
  EXPECT_EQ(s_counter, 1);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, PostDelayedTaskMultiple) {
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(40));

  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(30));

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            2u);

  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            1u);

  EXPECT_EQ(s_counter, 1);

  task_environment_.FastForwardBy(base::Seconds(10));

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            0u);

  EXPECT_EQ(s_counter, 2);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, PostDelayedTaskPreFreeze) {
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(60));

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            1u);

  task_environment_.FastForwardBy(base::Seconds(30));

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            0u);
  EXPECT_EQ(s_counter, 1);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, PostDelayedTaskMultiThreaded) {
  base::WaitableEvent event1(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent event2(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  ASSERT_FALSE(task_runner->RunsTasksInCurrentSequence());

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<base::SequencedTaskRunner> task_runner,
             base::WaitableEvent* event1, base::WaitableEvent* event2) {
            PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
                task_runner, FROM_HERE,
                base::BindOnce(
                    [](base::WaitableEvent* event) {
                      IncGlobalCounter();
                      event->Signal();
                    },
                    base::Unretained(event2)),
                base::Seconds(30));
            event1->Signal();
          },
          task_runner, base::Unretained(&event1), base::Unretained(&event2)));

  task_environment_.FastForwardBy(base::Seconds(1));

  event1.Wait();

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            1u);

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  event2.Wait();

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            0u);

  EXPECT_EQ(s_counter, 1);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest,
       PostDelayedTaskBeforeAndAfterPreFreeze) {
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(60));

  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(30));

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            2u);

  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            1u);
  EXPECT_EQ(s_counter, 1);

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            0u);
  EXPECT_EQ(s_counter, 2);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, AddDuringPreFreeze) {
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&PostDelayedIncGlobal), base::Seconds(10));

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            1u);

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(PreFreezeBackgroundMemoryTrimmer::Instance()
                .GetNumberOfPendingBackgroundTasksForTesting(),
            0u);

  EXPECT_EQ(s_counter, 1);
}

}  // namespace base
