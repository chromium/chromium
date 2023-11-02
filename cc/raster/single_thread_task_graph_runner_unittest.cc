// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/simple_thread.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "cc/test/task_graph_runner_test_template.h"

namespace cc {
namespace {

class SingleThreadTaskGraphRunnerTestDelegate {
 public:
  SingleThreadTaskGraphRunnerTestDelegate() = default;

  void StartTaskGraphRunner() {
    single_thread_task_graph_runner_.Start(
        "SingleThreadTaskGraphRunnerTestDelegate",
        base::SimpleThread::Options());
  }

  TaskGraphRunner* GetTaskGraphRunner() {
    return &single_thread_task_graph_runner_;
  }

  void StopTaskGraphRunner() {}

  ~SingleThreadTaskGraphRunnerTestDelegate() {
    single_thread_task_graph_runner_.Shutdown();
  }

 private:
  SingleThreadTaskGraphRunner single_thread_task_graph_runner_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(SingleThreadTaskGraphRunner,
                               TaskGraphRunnerTest,
                               SingleThreadTaskGraphRunnerTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(SingleThreadTaskGraphRunner,
                               SingleThreadTaskGraphRunnerTest,
                               SingleThreadTaskGraphRunnerTestDelegate);

}  // namespace
}  // namespace cc
