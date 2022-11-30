// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/simple_thread.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/test/task_graph_runner_test_template.h"

namespace cc {
namespace {

class SynchronousTaskGraphRunnerTestDelegate {
 public:
  SynchronousTaskGraphRunnerTestDelegate() = default;

  void StartTaskGraphRunner() {}

  TaskGraphRunner* GetTaskGraphRunner() {
    return &synchronous_task_graph_runner_;
  }

  void StopTaskGraphRunner() { synchronous_task_graph_runner_.RunUntilIdle(); }

 private:
  SynchronousTaskGraphRunner synchronous_task_graph_runner_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(SynchronousTaskGraphRunner,
                               TaskGraphRunnerTest,
                               SynchronousTaskGraphRunnerTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(SynchronousTaskGraphRunner,
                               SingleThreadTaskGraphRunnerTest,
                               SynchronousTaskGraphRunnerTestDelegate);

}  // namespace
}  // namespace cc
