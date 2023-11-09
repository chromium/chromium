// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_apple.h"

#include "base/apple/scoped_cftyperef.h"
#include "base/cancelable_callback.h"
#include "base/functional/bind.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(MessagePumpAppleTest, QuitWithModalWindow) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  window.releasedWhenClosed = NO;

  // Check that quitting the run loop while a modal window is shown applies to
  // |run_loop| rather than the internal NSApplication modal run loop.
  RunLoop run_loop;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
        [NSApp runModalForWindow:window];
      }));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        [NSApp stopModal];
        run_loop.Quit();
      }));

  EXPECT_NO_FATAL_FAILURE(run_loop.Run());
}

}  // namespace base
