// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_MOCK_TIME_MESSAGE_LOOP_TASK_RUNNER_H_
#define BASE_TEST_SCOPED_MOCK_TIME_MESSAGE_LOOP_TASK_RUNNER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_mock_time_task_runner.h"

namespace base {

class SingleThreadTaskRunner;

// A scoped wrapper around TestMockTimeTaskRunner that replaces
// MessageLoopCurrent::Get()'s task runner (and consequently
// ThreadTaskRunnerHandle) with a TestMockTimeTaskRunner and resets it back at
// the end of its scope.
//
// Note: RunLoop() will not work in the scope of a
// ScopedMockTimeMessageLoopTaskRunner, the underlying TestMockTimeTaskRunner's
// methods must be used instead to pump tasks.
//
// Note: Use TaskEnvironment + TimeSource::MOCK_TIME instead of this in unit
// tests. In browser tests you unfortunately still need this at the moment to
// mock delayed tasks on the main thread...
class ScopedMockTimeMessageLoopTaskRunner {
 public:
  ScopedMockTimeMessageLoopTaskRunner();
  ~ScopedMockTimeMessageLoopTaskRunner();

  TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }
  TestMockTimeTaskRunner* operator->() { return task_runner_.get(); }

 private:
  const scoped_refptr<TestMockTimeTaskRunner> task_runner_;
  scoped_refptr<SingleThreadTaskRunner> previous_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMockTimeMessageLoopTaskRunner);
};

}  // namespace base

#endif  // BASE_TEST_SCOPED_MOCK_TIME_MESSAGE_LOOP_TASK_RUNNER_H_
