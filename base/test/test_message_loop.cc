// Copyright 2016 The Chromium Authors
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
#if BUILDFLAG(IS_ANDROID)
    case MessagePumpType::JAVA:
#elif BUILDFLAG(IS_APPLE)
    case MessagePumpType::NS_RUNLOOP:
#endif
      NOTREACHED();
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
