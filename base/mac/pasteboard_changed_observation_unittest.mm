// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/pasteboard_changed_observation.h"

#import <AppKit/AppKit.h>
#include <dispatch/dispatch.h>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "testing/platform_test.h"

namespace base {
namespace {

using PasteboardChangedObservationTest = PlatformTest;

TEST_F(PasteboardChangedObservationTest, TestTriggers) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedRunLoopTimeout timeout_{FROM_HERE, base::Seconds(2)};
  RunLoop run_loop;

  bool callback_called = false;
  CallbackListSubscription subscription = RegisterPasteboardChangedCallback(
      BindLambdaForTesting([&callback_called, &run_loop] {
        callback_called = true;
        run_loop.Quit();
      }));

  [NSPasteboard.generalPasteboard clearContents];
  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

TEST_F(PasteboardChangedObservationTest, TestDoesntTrigger) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedRunLoopTimeout timeout_{FROM_HERE, base::Seconds(5)};
  RunLoop run_loop;

  int callbacks = 0;
  CallbackListSubscription subscription = RegisterPasteboardChangedCallback(
      BindLambdaForTesting([&callbacks] { ++callbacks; }));

  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, BindLambdaForTesting([&run_loop] { run_loop.Quit(); }),
      base::Seconds(3));

  run_loop.Run();
  // Three is arbitrary, but expecting zero is asking for flakiness.
  EXPECT_LE(callbacks, 3);
}

// The swizzle outlives the main thread's task runner, so a pasteboard change
// during shutdown must not crash.
TEST_F(PasteboardChangedObservationTest, TestChangeAfterMainThreadShutdown) {
  NSPasteboard* pasteboard;
  {
    base::test::SingleThreadTaskEnvironment task_environment;
    base::test::ScopedRunLoopTimeout timeout_{FROM_HERE, base::Seconds(2)};
    // Use a private pasteboard so that changing it doesn't invalidate the
    // pasteboard caches of other processes.
    pasteboard = [NSPasteboard pasteboardWithUniqueName];
    RunLoop run_loop;
    CallbackListSubscription subscription =
        RegisterPasteboardChangedCallback(run_loop.QuitClosure());
    [pasteboard clearContents];
    run_loop.Run();
  }
  ASSERT_FALSE(SingleThreadTaskRunner::HasMainThreadDefault());

  // Pasteboard changes are processed on an internal CFPasteboard dispatch
  // queue, so make this change from a thread other than the main thread.
  dispatch_semaphore_t done = dispatch_semaphore_create(0);
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    [pasteboard clearContents];
    dispatch_semaphore_signal(done);
  });
  dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);

  [pasteboard releaseGlobally];
}

}  // namespace
}  // namespace base
