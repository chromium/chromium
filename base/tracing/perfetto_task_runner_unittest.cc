// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracing/perfetto_task_runner.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)
#include <sys/socket.h>
#include <sys/types.h>
#include "base/posix/eintr_wrapper.h"
#endif

namespace base {
namespace tracing {
namespace {

class TaskDestination {
 public:
  TaskDestination(size_t number_of_sequences,
                  size_t expected_tasks,
                  base::OnceClosure on_complete)
      : expected_tasks_(expected_tasks),
        on_complete_(std::move(on_complete)),
        last_task_id_(number_of_sequences) {}

  size_t tasks_run() const { return tasks_run_; }

  void TestTask(int n, size_t sequence_number = 0) {
    EXPECT_LT(sequence_number, last_task_id_.size());
    EXPECT_GT(expected_tasks_, tasks_run_);
    EXPECT_GE(n, last_task_id_[sequence_number]);
    last_task_id_[sequence_number] = n;

    if (++tasks_run_ == expected_tasks_) {
      std::move(on_complete_).Run();
    }
  }

  base::WeakPtr<TaskDestination> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const size_t expected_tasks_;
  base::OnceClosure on_complete_;
  std::vector<int> last_task_id_;
  size_t tasks_run_ = 0;

  base::WeakPtrFactory<TaskDestination> weak_ptr_factory_{this};
};

class PosterThread : public base::SimpleThread {
 public:
  PosterThread(PerfettoTaskRunner* task_runner,
               base::WeakPtr<TaskDestination> weak_ptr,
               int n,
               size_t sequence_number)
      : SimpleThread("TaskPostThread"),
        task_runner_(task_runner),
        weak_ptr_(weak_ptr),
        n_(n),
        sequence_number_(sequence_number) {}
  ~PosterThread() override = default;

  // base::SimpleThread overrides.
  void BeforeStart() override {}
  void BeforeJoin() override {}

  void Run() override {
    for (int i = 0; i < n_; ++i) {
      auto weak_ptr = weak_ptr_;
      auto sequence_number = sequence_number_;
      task_runner_->PostTask([weak_ptr, i, sequence_number] {
        weak_ptr->TestTask(i, sequence_number);
      });
    }
  }

 private:
  raw_ptr<PerfettoTaskRunner> task_runner_;
  base::WeakPtr<TaskDestination> weak_ptr_;
  const int n_;
  const size_t sequence_number_;
};

class PerfettoTaskRunnerTest : public testing::Test {
 public:
  void SetUp() override {
    sequenced_task_runner_ = CreateNewTaskrunner();
    task_runner_ = std::make_unique<PerfettoTaskRunner>(sequenced_task_runner_);
  }

  scoped_refptr<base::SequencedTaskRunner> CreateNewTaskrunner() {
    return base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock()}, base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  }
  void SetTaskExpectations(base::OnceClosure on_complete,
                           size_t expected_tasks,
                           size_t number_of_sequences = 1) {
    task_destination_ = std::make_unique<TaskDestination>(
        number_of_sequences, expected_tasks, std::move(on_complete));
  }

  void TearDown() override {
    sequenced_task_runner_->DeleteSoon(FROM_HERE, std::move(task_runner_));
  }

  PerfettoTaskRunner* task_runner() { return task_runner_.get(); }
  TaskDestination* destination() { return task_destination_.get(); }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  std::unique_ptr<PerfettoTaskRunner> task_runner_;
  std::unique_ptr<TaskDestination> task_destination_;
};

TEST_F(PerfettoTaskRunnerTest, SequentialTasks) {
  base::RunLoop wait_for_tasks;
  SetTaskExpectations(wait_for_tasks.QuitClosure(), 3);

  auto weak_ptr = destination()->GetWeakPtr();
  for (int i = 1; i <= 3; ++i) {
    task_runner()->PostTask([=]() mutable {
      auto* dest = weak_ptr.get();
      // The weak pointer must be reset before TestTask() is called, otherwise
      // there will be a race where the factory could be destructed on main
      // thread while still bound to the task runner sequence.
      weak_ptr.reset();
      dest->TestTask(i);
    });
  }

  wait_for_tasks.Run();
}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL)
// Tests file descriptor reuse that causes crashes.
TEST_F(PerfettoTaskRunnerTest, FileDescriptorReuse) {
  int sockets[2];
  // Use sockets because we need a FD that supports epoll().
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
  base::ScopedFD fd(sockets[0]), write_fd(sockets[1]);
  ASSERT_TRUE(fd.is_valid());

  constexpr int data_value = 0x12ab34cd;
  bool run_callback_1 = false, run_callback_2 = false;
  int data = data_value;
  constexpr ssize_t data_size = static_cast<ssize_t>(sizeof(data));

  // Trigger the file descriptor watcher callback.
  ASSERT_EQ(data_size, HANDLE_EINTR(write(write_fd.get(), &data, data_size)));
  data = 0;

  base::RunLoop run_loop;

  task_runner()->GetOrCreateTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        // The 1st add operation posts a task.
        task_runner()->AddFileDescriptorWatch(fd.get(), [&] {
          run_callback_1 = true;
          ASSERT_EQ(data_size, HANDLE_EINTR(read(fd.get(), &data, data_size)));
          run_loop.Quit();
        });
        // Remove so the 2nd add operation can succeed.
        task_runner()->RemoveFileDescriptorWatch(fd.get());

        // Simulate FD reuse. The 2nd add operation also posts a task.
        task_runner()->AddFileDescriptorWatch(fd.get(), [&] {
          run_callback_2 = true;
          ASSERT_EQ(data_size, HANDLE_EINTR(read(fd.get(), &data, data_size)));
          run_loop.Quit();
        });
      }));

  // Make all posted tasks run.
  run_loop.Run();

  ASSERT_FALSE(run_callback_1);
  ASSERT_TRUE(run_callback_2);
  ASSERT_EQ(data, data_value);

  task_runner()->GetOrCreateTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        // Cleanup the FD watcher.
        task_runner()->RemoveFileDescriptorWatch(fd.get());
      }));
  task_environment().RunUntilIdle();
}
#endif
}  // namespace
}  // namespace tracing
}  // namespace base
