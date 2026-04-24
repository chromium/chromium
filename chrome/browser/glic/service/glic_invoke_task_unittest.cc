// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_task.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

class MockTask : public GlicInvokeTask {
 public:
  explicit MockTask(base::OnceClosure on_start, bool complete_sync = true)
      : on_start_(std::move(on_start)), complete_sync_(complete_sync) {}
  ~MockTask() override = default;

  void Start(base::OnceClosure done_callback) override {
    std::move(on_start_).Run();
    if (complete_sync_) {
      std::move(done_callback).Run();
    } else {
      done_callback_ = std::move(done_callback);
    }
  }

  void Complete() { std::move(done_callback_).Run(); }

 private:
  base::OnceClosure on_start_;
  bool complete_sync_;
  base::OnceClosure done_callback_;
};

}  // namespace

TEST(GlicInvokeTaskTest, EmptySequentialTask) {
  SequentialTaskGroup seq;
  base::MockCallback<base::OnceClosure> done_cb;
  EXPECT_CALL(done_cb, Run()).Times(1);
  seq.Start(done_cb.Get());
}

TEST(GlicInvokeTaskTest, EmptyParallelTask) {
  ParallelTaskGroup par;
  base::MockCallback<base::OnceClosure> done_cb;
  EXPECT_CALL(done_cb, Run()).Times(1);
  par.Start(done_cb.Get());
}

TEST(GlicInvokeTaskTest, SequentialExecution) {
  base::MockCallback<base::OnceClosure> start_cb1;
  base::MockCallback<base::OnceClosure> start_cb2;
  base::MockCallback<base::OnceClosure> done_cb;

  auto task1 = std::make_unique<MockTask>(start_cb1.Get(), false);
  auto task2 = std::make_unique<MockTask>(start_cb2.Get(), true);

  MockTask* task1_ptr = task1.get();

  std::vector<std::unique_ptr<GlicInvokeTask>> tasks;
  tasks.push_back(std::move(task1));
  tasks.push_back(std::move(task2));
  SequentialTaskGroup seq(std::move(tasks));

  EXPECT_CALL(start_cb1, Run()).Times(1);
  EXPECT_CALL(start_cb2, Run()).Times(0);
  EXPECT_CALL(done_cb, Run()).Times(0);

  seq.Start(done_cb.Get());

  // Task 2 should only start after Task 1 completes.
  EXPECT_CALL(start_cb2, Run()).Times(1);
  EXPECT_CALL(done_cb, Run()).Times(1);
  task1_ptr->Complete();
}

TEST(GlicInvokeTaskTest, ParallelExecution) {
  base::MockCallback<base::OnceClosure> start_cb1;
  base::MockCallback<base::OnceClosure> start_cb2;
  base::MockCallback<base::OnceClosure> done_cb;

  auto task1 = std::make_unique<MockTask>(start_cb1.Get(), false);
  auto task2 = std::make_unique<MockTask>(start_cb2.Get(), false);

  MockTask* task1_ptr = task1.get();
  MockTask* task2_ptr = task2.get();

  std::vector<std::unique_ptr<GlicInvokeTask>> tasks;
  tasks.push_back(std::move(task1));
  tasks.push_back(std::move(task2));
  ParallelTaskGroup par(std::move(tasks));

  EXPECT_CALL(start_cb1, Run()).Times(1);
  EXPECT_CALL(start_cb2, Run()).Times(1);
  EXPECT_CALL(done_cb, Run()).Times(0);

  par.Start(done_cb.Get());

  // Done should only be called after BOTH tasks complete.
  task1_ptr->Complete();
  EXPECT_CALL(done_cb, Run()).Times(1);
  task2_ptr->Complete();
}

TEST(GlicInvokeTaskTest, NestedSequentialInParallel) {
  base::MockCallback<base::OnceClosure> start_cb1a;
  base::MockCallback<base::OnceClosure> start_cb1b;
  base::MockCallback<base::OnceClosure> start_cb2a;
  base::MockCallback<base::OnceClosure> done_cb;

  auto task1a = std::make_unique<MockTask>(start_cb1a.Get(), false);
  auto task1b = std::make_unique<MockTask>(start_cb1b.Get(), true);
  auto task2a = std::make_unique<MockTask>(start_cb2a.Get(), true);

  MockTask* task1a_ptr = task1a.get();

  std::vector<std::unique_ptr<GlicInvokeTask>> seq1_tasks;
  seq1_tasks.push_back(std::move(task1a));
  seq1_tasks.push_back(std::move(task1b));
  auto seq1 = std::make_unique<SequentialTaskGroup>(std::move(seq1_tasks));

  std::vector<std::unique_ptr<GlicInvokeTask>> seq2_tasks;
  seq2_tasks.push_back(std::move(task2a));
  auto seq2 = std::make_unique<SequentialTaskGroup>(std::move(seq2_tasks));

  std::vector<std::unique_ptr<GlicInvokeTask>> par_tasks;
  par_tasks.push_back(std::move(seq1));
  par_tasks.push_back(std::move(seq2));
  ParallelTaskGroup par(std::move(par_tasks));

  EXPECT_CALL(start_cb1a, Run()).Times(1);
  EXPECT_CALL(start_cb2a, Run()).Times(1);
  EXPECT_CALL(start_cb1b, Run()).Times(0);
  EXPECT_CALL(done_cb, Run()).Times(0);

  par.Start(done_cb.Get());

  // seq2 completes immediately (task2a is sync).
  // seq1 is waiting for task1a.
  EXPECT_CALL(start_cb1b, Run()).Times(1);
  EXPECT_CALL(done_cb, Run()).Times(1);
  task1a_ptr->Complete();
}

TEST(GlicInvokeTaskTest, NestedParallelInSequential) {
  base::MockCallback<base::OnceClosure> start_cb1a;
  base::MockCallback<base::OnceClosure> start_cb1b;
  base::MockCallback<base::OnceClosure> start_cb2a;
  base::MockCallback<base::OnceClosure> done_cb;

  auto task1a = std::make_unique<MockTask>(start_cb1a.Get(), false);
  auto task1b = std::make_unique<MockTask>(start_cb1b.Get(), true);
  auto task2a = std::make_unique<MockTask>(start_cb2a.Get(), true);

  MockTask* task1a_ptr = task1a.get();

  std::vector<std::unique_ptr<GlicInvokeTask>> par1_tasks;
  par1_tasks.push_back(std::move(task1a));
  par1_tasks.push_back(std::move(task1b));
  auto par1 = std::make_unique<ParallelTaskGroup>(std::move(par1_tasks));

  std::vector<std::unique_ptr<GlicInvokeTask>> par2_tasks;
  par2_tasks.push_back(std::move(task2a));
  auto par2 = std::make_unique<ParallelTaskGroup>(std::move(par2_tasks));

  std::vector<std::unique_ptr<GlicInvokeTask>> seq_tasks;
  seq_tasks.push_back(std::move(par1));
  seq_tasks.push_back(std::move(par2));
  SequentialTaskGroup seq(std::move(seq_tasks));

  // First parallel task starts both.
  EXPECT_CALL(start_cb1a, Run()).Times(1);
  EXPECT_CALL(start_cb1b, Run()).Times(1);
  EXPECT_CALL(start_cb2a, Run()).Times(0);
  EXPECT_CALL(done_cb, Run()).Times(0);

  seq.Start(done_cb.Get());

  // Task 1a completes, finishing par1, which triggers seq to start par2.
  EXPECT_CALL(start_cb2a, Run()).Times(1);
  EXPECT_CALL(done_cb, Run()).Times(1);
  task1a_ptr->Complete();
}

TEST(GlicInvokeTaskTest, PostCallbackTask) {
  base::test::TaskEnvironment task_environment;
  base::RunLoop run_loop;

  base::MockCallback<base::OnceClosure> done_cb;
  base::MockCallback<base::OnceClosure> mock_callback;

  PostCallbackTask task(base::BindOnce(
      [](base::OnceClosure mock_cb, base::OnceClosure quit_cb) {
        std::move(mock_cb).Run();
        std::move(quit_cb).Run();
      },
      mock_callback.Get(), run_loop.QuitClosure()));

  EXPECT_CALL(mock_callback, Run()).Times(0);
  EXPECT_CALL(done_cb, Run()).Times(1);

  task.Start(done_cb.Get());

  // The callback should not have run yet because it was posted.
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback, Run()).Times(1);
  run_loop.Run();
}

}  // namespace glic
