// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/queue.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/run_loop.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace {

void OnAbort(int* abort_counter) {
  ++(*abort_counter);
}

AbortCallback OnRun(int* run_counter, int* abort_counter) {
  ++(*run_counter);
  return base::Bind(&OnAbort, abort_counter);
}

#if !defined(NDEBUG) && defined(GTEST_HAS_DEATH_TEST)

AbortCallback OnRunNonAbortable(int* run_counter, int* abort_counter) {
  ++(*run_counter);
  return AbortCallback();
}

#endif

}  // namespace

class FileSystemProviderQueueTest : public testing::Test {
 protected:
  FileSystemProviderQueueTest() {}
  ~FileSystemProviderQueueTest() override {}

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(FileSystemProviderQueueTest, NewToken) {
  Queue queue(1);
  EXPECT_EQ(1u, queue.NewToken());
  EXPECT_EQ(2u, queue.NewToken());
  EXPECT_EQ(3u, queue.NewToken());
}

TEST_F(FileSystemProviderQueueTest, Enqueue_OneAtOnce) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  const size_t second_token = queue.NewToken();
  int second_counter = 0;
  int second_abort_counter = 0;
  queue.Enqueue(second_token,
                base::Bind(&OnRun, &second_counter, &second_abort_counter));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(0, second_counter);
  EXPECT_EQ(0, second_abort_counter);

  // Complete the first task from the queue should run the second task.
  queue.Complete(first_token);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(1, second_counter);
  EXPECT_EQ(0, second_abort_counter);

  const size_t third_token = queue.NewToken();
  int third_counter = 0;
  int third_abort_counter = 0;
  queue.Enqueue(third_token,
                base::Bind(&OnRun, &third_counter, &third_abort_counter));

  // The second task is still running, so the third one is blocked.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(1, second_counter);
  EXPECT_EQ(0, second_abort_counter);
  EXPECT_EQ(0, third_counter);
  EXPECT_EQ(0, third_abort_counter);

  // After aborting the second task, the third should run.
  queue.Abort(second_token);
  queue.Complete(second_token);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(1, second_counter);
  EXPECT_EQ(1, second_abort_counter);
  EXPECT_EQ(1, third_counter);
  EXPECT_EQ(0, third_abort_counter);
}

TEST_F(FileSystemProviderQueueTest, Enqueue_MultipleAtOnce) {
  Queue queue(2);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  const size_t second_token = queue.NewToken();
  int second_counter = 0;
  int second_abort_counter = 0;
  queue.Enqueue(second_token,
                base::Bind(&OnRun, &second_counter, &second_abort_counter));

  const size_t third_token = queue.NewToken();
  int third_counter = 0;
  int third_abort_counter = 0;
  queue.Enqueue(third_token,
                base::Bind(&OnRun, &third_counter, &third_abort_counter));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(1, second_counter);
  EXPECT_EQ(0, second_abort_counter);
  EXPECT_EQ(0, third_counter);
  EXPECT_EQ(0, third_abort_counter);

  // Completing and removing the second task, should start the last one.
  queue.Complete(second_token);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(1, second_counter);
  EXPECT_EQ(0, second_abort_counter);
  EXPECT_EQ(1, third_counter);
  EXPECT_EQ(0, third_abort_counter);
}

#if !defined(NDEBUG) && defined(GTEST_HAS_DEATH_TEST)

TEST_F(FileSystemProviderQueueTest, InvalidUsage_DuplicatedTokens) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  // Use the first token on purpose.
  int second_counter = 0;
  int second_abort_counter = 0;
  EXPECT_DEATH(queue.Enqueue(first_token, base::Bind(&OnRun, &second_counter,
                                                     &second_abort_counter)),
               "");
}

TEST_F(FileSystemProviderQueueTest, InvalidUsage_CompleteNotStarted) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  // Completing and removing the first task, which however hasn't started.
  // That should not invoke the second task.
  EXPECT_DEATH(queue.Complete(first_token), "");
}

TEST_F(FileSystemProviderQueueTest,
       InvalidUsage_CompleteAfterAbortingNonExecutedTask) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  std::vector<base::File::Error> first_abort_callback_log;
  queue.Abort(first_token);

  EXPECT_DEATH(queue.Complete(first_token), "");
}

TEST_F(FileSystemProviderQueueTest, InvalidUsage_AbortAfterCompleting) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  base::RunLoop().RunUntilIdle();

  queue.Complete(first_token);
  EXPECT_DEATH(queue.Abort(first_token), "");
}

TEST_F(FileSystemProviderQueueTest, InvalidUsage_CompleteTwice) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  base::RunLoop().RunUntilIdle();

  queue.Complete(first_token);
  EXPECT_DEATH(queue.Complete(first_token), "");
}

TEST_F(FileSystemProviderQueueTest, InvalidUsage_AbortTwice) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  base::RunLoop().RunUntilIdle();

  queue.Abort(first_token);
  EXPECT_DEATH(queue.Abort(first_token), "");
}

TEST_F(FileSystemProviderQueueTest, InvalidUsage_AbortNonAbortable) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token, base::Bind(&OnRunNonAbortable, &first_counter,
                                        &first_abort_counter));

  base::RunLoop().RunUntilIdle();

  EXPECT_DEATH(queue.Abort(first_token), "");
}

#endif

TEST_F(FileSystemProviderQueueTest, Enqueue_Abort) {
  Queue queue(1);
  const size_t first_token = queue.NewToken();
  int first_counter = 0;
  int first_abort_counter = 0;
  queue.Enqueue(first_token,
                base::Bind(&OnRun, &first_counter, &first_abort_counter));

  const size_t second_token = queue.NewToken();
  int second_counter = 0;
  int second_abort_counter = 0;
  queue.Enqueue(second_token,
                base::Bind(&OnRun, &second_counter, &second_abort_counter));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(0, first_abort_counter);
  EXPECT_EQ(0, second_counter);
  EXPECT_EQ(0, second_abort_counter);

  // Abort the first task while it's being executed.
  queue.Abort(first_token);
  queue.Complete(first_token);

  // Abort the second task, before it's started.
  EXPECT_EQ(0, second_counter);
  queue.Abort(second_token);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, first_counter);
  EXPECT_EQ(1, first_abort_counter);
  EXPECT_EQ(0, second_counter);
  EXPECT_EQ(0, second_abort_counter);
}

}  // namespace file_system_provider
}  // namespace chromeos
