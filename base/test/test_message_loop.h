// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_MESSAGE_LOOP_H_
#define BASE_TEST_TEST_MESSAGE_LOOP_H_

#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_type.h"
#include "base/single_thread_task_runner.h"

namespace base {

// TestMessageLoop is a convenience class for unittests that need to create a
// message loop without a real thread backing it. For most tests,
// it is sufficient to just instantiate TestMessageLoop as a member variable.
//
// TestMessageLoop will attempt to drain the underlying MessageLoop on
// destruction for clean teardown of tests.
class TestMessageLoop {
 public:
  TestMessageLoop();
  explicit TestMessageLoop(MessagePumpType type);
  ~TestMessageLoop();

  scoped_refptr<SingleThreadTaskRunner> task_runner() {
    return loop_.task_runner();
  }

 private:
  MessageLoop loop_;
};

}  // namespace base

#endif  // BASE_TEST_TEST_MESSAGE_LOOP_H_
