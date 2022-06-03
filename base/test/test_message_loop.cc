// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_message_loop.h"

#include "base/compiler_specific.h"
#include "base/message_loop/message_pump_type.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"

namespace base {

namespace {

test::SingleThreadTaskEnvironment::MainThreadType GetMainThreadType(
    MessagePumpType type) {
  switch (type) {
    case MessagePumpType::DEFAULT:
      return test::SingleThreadTaskEnvironment::MainThreadType::DEFAULT;
    case MessagePumpType::IO:
      return test::SingleThreadTaskEnvironment::MainThreadType::IO;
    case MessagePumpType::UI:
      return test::SingleThreadTaskEnvironment::MainThreadType::UI;
    case MessagePumpType::CUSTOM:
#if defined(OS_ANDROID)
    case MessagePumpType::JAVA:
#elif defined(OS_APPLE)
    case MessagePumpType::NS_RUNLOOP:
#elif defined(OS_WIN)
    case MessagePumpType::UI_WITH_WM_QUIT_SUPPORT:
#endif
      NOTREACHED();
      return test::SingleThreadTaskEnvironment::MainThreadType::DEFAULT;
  }
}
}  // namespace

TestMessageLoop::TestMessageLoop() = default;

TestMessageLoop::TestMessageLoop(MessagePumpType type)
    : task_environment_(GetMainThreadType(type)) {}

TestMessageLoop::~TestMessageLoop() {
  RunLoop().RunUntilIdle();
}

}  // namespace base
