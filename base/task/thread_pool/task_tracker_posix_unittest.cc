// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/task_tracker_posix.h"

#include <unistd.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/sequence_token.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/task.h"
#include "base/task/thread_pool/test_utils.h"
#include "base/test/null_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

class ThreadPoolTaskTrackerPosixTest : public testing::Test {
 public:
  ThreadPoolTaskTrackerPosixTest() : service_thread_("ServiceThread") {
    Thread::Options service_thread_options;
    service_thread_options.message_pump_type = MessagePumpType::IO;
    service_thread_.StartWithOptions(service_thread_options);
    tracker_.set_io_thread_task_runner(service_thread_.task_runner());
  }

 protected:
  Thread service_thread_;
  TaskTrackerPosix tracker_{"Test"};

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadPoolTaskTrackerPosixTest);
};

}  // namespace

// Verify that TaskTrackerPosix runs a Task it receives.
TEST_F(ThreadPoolTaskTrackerPosixTest, RunTask) {
  bool did_run = false;
  Task task(
      FROM_HERE,
      BindOnce([](bool* did_run) { *did_run = true; }, Unretained(&did_run)),
      TimeDelta());
  constexpr TaskTraits default_traits = {ThreadPool()};

  EXPECT_TRUE(tracker_.WillPostTask(&task, default_traits.shutdown_behavior()));

  auto sequence = test::CreateSequenceWithTask(std::move(task), default_traits);
  // Expect RunAndPopNextTask to return nullptr since |sequence| is empty after
  // popping a task from it.
  EXPECT_FALSE(test::QueueAndRunTaskSource(&tracker_, std::move(sequence)));

  EXPECT_TRUE(did_run);
}

// Verify that FileDescriptorWatcher::WatchReadable() can be called from a task
// running in TaskTrackerPosix without a crash.
TEST_F(ThreadPoolTaskTrackerPosixTest, FileDescriptorWatcher) {
  int fds[2];
  ASSERT_EQ(0, pipe(fds));
  Task task(FROM_HERE,
            BindOnce(IgnoreResult(&FileDescriptorWatcher::WatchReadable),
                     fds[0], DoNothing()),
            TimeDelta());
  constexpr TaskTraits default_traits = {ThreadPool()};

  EXPECT_TRUE(tracker_.WillPostTask(&task, default_traits.shutdown_behavior()));

  // FileDescriptorWatcher::WatchReadable needs a SequencedTaskRunnerHandle.
  auto sequence = test::CreateSequenceWithTask(
      std::move(task), default_traits, MakeRefCounted<NullTaskRunner>(),
      TaskSourceExecutionMode::kSequenced);

  // Expect RunAndPopNextTask to return nullptr since |sequence| is empty after
  // popping a task from it.
  EXPECT_FALSE(test::QueueAndRunTaskSource(&tracker_, std::move(sequence)));

  // Join the service thread to make sure that the read watch is registered and
  // unregistered before file descriptors are closed.
  service_thread_.Stop();

  EXPECT_EQ(0, IGNORE_EINTR(close(fds[0])));
  EXPECT_EQ(0, IGNORE_EINTR(close(fds[1])));
}

}  // namespace internal
}  // namespace base
