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

@interface TestModalAlertCloser : NSObject
- (void)runTestThenCloseAlert:(NSAlert*)alert;
@end

namespace {

// Internal constants from message_pump_apple.mm.
constexpr int kAllModesMask = 0b0000'0111;
constexpr int kNSApplicationModalSafeModeMask = 0b0000'0001;

}  // namespace

namespace base {

namespace {

// PostedTasks are only executed while the message pump has a delegate. That is,
// when a base::RunLoop is running, so in order to test whether posted tasks
// are run by CFRunLoopRunInMode and *not* by the regular RunLoop, we need to
// be inside a task that is also calling CFRunLoopRunInMode.
// This function posts |task| and runs the given |mode|.
void RunTaskInMode(CFRunLoopMode mode, OnceClosure task) {
  // Since this task is "ours" rather than a system task, allow nesting.
  CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
  CancelableOnceClosure cancelable(std::move(task));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                        cancelable.callback());
  while (CFRunLoopRunInMode(mode, 0, true) == kCFRunLoopRunHandledSource)
    ;
}

}  // namespace

// Tests the correct behavior of ScopedPumpMessagesInPrivateModes.
TEST(MessagePumpAppleTest, ScopedPumpMessagesInPrivateModes) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  CFRunLoopMode kRegular = kCFRunLoopDefaultMode;
  CFRunLoopMode kPrivate = CFSTR("NSUnhighlightMenuRunLoopMode");

  // Work is seen when running in the default mode.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(&RunTaskInMode, kRegular, MakeExpectedRunClosure(FROM_HERE)));
  EXPECT_NO_FATAL_FAILURE(RunLoop().RunUntilIdle());

  // But not seen when running in a private mode.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(&RunTaskInMode, kPrivate, MakeExpectedNotRunClosure(FROM_HERE)));
  EXPECT_NO_FATAL_FAILURE(RunLoop().RunUntilIdle());

  {
    ScopedPumpMessagesInPrivateModes allow_private;
    // Now the work should be seen.
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        BindOnce(&RunTaskInMode, kPrivate, MakeExpectedRunClosure(FROM_HERE)));
    EXPECT_NO_FATAL_FAILURE(RunLoop().RunUntilIdle());

    // The regular mode should also work the same.
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        BindOnce(&RunTaskInMode, kRegular, MakeExpectedRunClosure(FROM_HERE)));
    EXPECT_NO_FATAL_FAILURE(RunLoop().RunUntilIdle());
  }

  // And now the scoper is out of scope, private modes should no longer see it.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(&RunTaskInMode, kPrivate, MakeExpectedNotRunClosure(FROM_HERE)));
  EXPECT_NO_FATAL_FAILURE(RunLoop().RunUntilIdle());

  // Only regular modes see it.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(&RunTaskInMode, kRegular, MakeExpectedRunClosure(FROM_HERE)));
  EXPECT_NO_FATAL_FAILURE(RunLoop().RunUntilIdle());
}

// Tests that private message loop modes are not pumped while a modal dialog is
// present.
TEST(MessagePumpAppleTest, ScopedPumpMessagesAttemptWithModalDialog) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  {
    base::ScopedPumpMessagesInPrivateModes allow_private;
    // No modal window, so all modes should be pumped.
    EXPECT_EQ(kAllModesMask, allow_private.GetModeMaskForTest());
  }

  NSAlert* alert = [[NSAlert alloc] init];
  [alert addButtonWithTitle:@"OK"];
  TestModalAlertCloser* closer = [[TestModalAlertCloser alloc] init];
  [closer performSelector:@selector(runTestThenCloseAlert:)
               withObject:alert
               afterDelay:0
                  inModes:@[ NSModalPanelRunLoopMode ]];
  NSInteger result = [alert runModal];
  EXPECT_EQ(NSAlertFirstButtonReturn, result);
}

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
        ScopedPumpMessagesInPrivateModes pump_private;
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

@implementation TestModalAlertCloser

- (void)runTestThenCloseAlert:(NSAlert*)alert {
  EXPECT_TRUE([NSApp modalWindow]);
  {
    base::ScopedPumpMessagesInPrivateModes allow_private;
    // With a modal window, only safe modes should be pumped.
    EXPECT_EQ(kNSApplicationModalSafeModeMask,
              allow_private.GetModeMaskForTest());
  }
  [[alert buttons][0] performClick:nil];
}

@end
