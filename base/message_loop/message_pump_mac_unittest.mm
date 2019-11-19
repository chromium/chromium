// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_mac.h"

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface TestModalAlertCloser : NSObject
- (void)runTestThenCloseAlert:(NSAlert*)alert;
@end

namespace {

// Internal constants from message_pump_mac.mm.
constexpr int kAllModesMask = 0xf;
constexpr int kNSApplicationModalSafeModeMask = 0x3;

}  // namespace

namespace base {

class TestMessagePumpCFRunLoopBase {
 public:
  bool TestCanInvalidateTimers() {
    return MessagePumpCFRunLoopBase::CanInvalidateCFRunLoopTimers();
  }
  static void SetTimerValid(CFRunLoopTimerRef timer, bool valid) {
    MessagePumpCFRunLoopBase::ChromeCFRunLoopTimerSetValid(timer, valid);
  }

  static void PerformTimerCallback(CFRunLoopTimerRef timer, void* info) {
    TestMessagePumpCFRunLoopBase* self =
        static_cast<TestMessagePumpCFRunLoopBase*>(info);
    self->timer_callback_called_ = true;

    if (self->invalidate_timer_in_callback_) {
      SetTimerValid(timer, false);
    }
  }

  bool invalidate_timer_in_callback_;

  bool timer_callback_called_;
};

TEST(MessagePumpMacTest, TestCanInvalidateTimers) {
  TestMessagePumpCFRunLoopBase message_pump_test;

  // Catch whether or not the use of private API ever starts failing.
  EXPECT_TRUE(message_pump_test.TestCanInvalidateTimers());
}

TEST(MessagePumpMacTest, TestInvalidatedTimerReuse) {
  TestMessagePumpCFRunLoopBase message_pump_test;

  CFRunLoopTimerContext timer_context = CFRunLoopTimerContext();
  timer_context.info = &message_pump_test;
  const CFTimeInterval kCFTimeIntervalMax =
      std::numeric_limits<CFTimeInterval>::max();
  ScopedCFTypeRef<CFRunLoopTimerRef> test_timer(CFRunLoopTimerCreate(
      NULL,                // allocator
      kCFTimeIntervalMax,  // fire time
      kCFTimeIntervalMax,  // interval
      0,                   // flags
      0,                   // priority
      TestMessagePumpCFRunLoopBase::PerformTimerCallback, &timer_context));
  CFRunLoopAddTimer(CFRunLoopGetCurrent(), test_timer,
                    kMessageLoopExclusiveRunLoopMode);

  // Sanity check.
  EXPECT_TRUE(CFRunLoopTimerIsValid(test_timer));

  // Confirm that the timer fires as expected, and that it's not a one-time-use
  // timer (those timers are invalidated after they fire).
  CFAbsoluteTime next_fire_time = CFAbsoluteTimeGetCurrent() + 0.01;
  CFRunLoopTimerSetNextFireDate(test_timer, next_fire_time);
  message_pump_test.timer_callback_called_ = false;
  message_pump_test.invalidate_timer_in_callback_ = false;
  CFRunLoopRunInMode(kMessageLoopExclusiveRunLoopMode, 0.02, true);
  EXPECT_TRUE(message_pump_test.timer_callback_called_);
  EXPECT_TRUE(CFRunLoopTimerIsValid(test_timer));

  // As a repeating timer, the timer should have a new fire date set in the
  // future.
  EXPECT_GT(CFRunLoopTimerGetNextFireDate(test_timer), next_fire_time);

  // Try firing the timer, and invalidating it within its callback.
  next_fire_time = CFAbsoluteTimeGetCurrent() + 0.01;
  CFRunLoopTimerSetNextFireDate(test_timer, next_fire_time);
  message_pump_test.timer_callback_called_ = false;
  message_pump_test.invalidate_timer_in_callback_ = true;
  CFRunLoopRunInMode(kMessageLoopExclusiveRunLoopMode, 0.02, true);
  EXPECT_TRUE(message_pump_test.timer_callback_called_);
  EXPECT_FALSE(CFRunLoopTimerIsValid(test_timer));

  // The CFRunLoop believes the timer is invalid, so it should not have a
  // fire date.
  EXPECT_EQ(0, CFRunLoopTimerGetNextFireDate(test_timer));

  // Now mark the timer as valid and confirm that it still fires correctly.
  TestMessagePumpCFRunLoopBase::SetTimerValid(test_timer, true);
  EXPECT_TRUE(CFRunLoopTimerIsValid(test_timer));
  next_fire_time = CFAbsoluteTimeGetCurrent() + 0.01;
  CFRunLoopTimerSetNextFireDate(test_timer, next_fire_time);
  message_pump_test.timer_callback_called_ = false;
  message_pump_test.invalidate_timer_in_callback_ = false;
  CFRunLoopRunInMode(kMessageLoopExclusiveRunLoopMode, 0.02, true);
  EXPECT_TRUE(message_pump_test.timer_callback_called_);
  EXPECT_TRUE(CFRunLoopTimerIsValid(test_timer));

  // Confirm that the run loop again gave it a new fire date in the future.
  EXPECT_GT(CFRunLoopTimerGetNextFireDate(test_timer), next_fire_time);

  CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), test_timer,
                       kMessageLoopExclusiveRunLoopMode);
}

namespace {

// PostedTasks are only executed while the message pump has a delegate. That is,
// when a base::RunLoop is running, so in order to test whether posted tasks
// are run by CFRunLoopRunInMode and *not* by the regular RunLoop, we need to
// be inside a task that is also calling CFRunLoopRunInMode.
// This function posts |task| and runs the given |mode|.
void RunTaskInMode(CFRunLoopMode mode, OnceClosure task) {
  // Since this task is "ours" rather than a system task, allow nesting.
  MessageLoopCurrent::ScopedNestableTaskAllower allow;
  CancelableOnceClosure cancelable(std::move(task));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, cancelable.callback());
  while (CFRunLoopRunInMode(mode, 0, true) == kCFRunLoopRunHandledSource)
    ;
}

}  // namespace

// Tests the correct behavior of ScopedPumpMessagesInPrivateModes.
TEST(MessagePumpMacTest, ScopedPumpMessagesInPrivateModes) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  CFRunLoopMode kRegular = kCFRunLoopDefaultMode;
  CFRunLoopMode kPrivate = CFSTR("NSUnhighlightMenuRunLoopMode");

  // Work is seen when running in the default mode.
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&RunTaskInMode, kRegular, MakeExpectedRunClosure(FROM_HERE)));
  EXPECT_NO_FATAL_FAILURE(RunLoop().RunUntilIdle());

  // But not seen when running in a private mode.
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&RunTaskInMode, kPrivate, MakeExpectedNotRunClosure(FROM_HERE)));
  EXPECT_NO_FATAL_FAILURE(RunLoop().RunUntilIdle());

  {
    ScopedPumpMessagesInPrivateModes allow_private;
    // Now the work should be seen.
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        BindOnce(&RunTaskInMode, kPrivate, MakeExpectedRunClosure(FROM_HERE)));
    EXPECT_NO_FATAL_FAILURE(RunLoop().RunUntilIdle());

    // The regular mode should also work the same.
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        BindOnce(&RunTaskInMode, kRegular, MakeExpectedRunClosure(FROM_HERE)));
    EXPECT_NO_FATAL_FAILURE(RunLoop().RunUntilIdle());
  }

  // And now the scoper is out of scope, private modes should no longer see it.
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&RunTaskInMode, kPrivate, MakeExpectedNotRunClosure(FROM_HERE)));
  EXPECT_NO_FATAL_FAILURE(RunLoop().RunUntilIdle());

  // Only regular modes see it.
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(&RunTaskInMode, kRegular, MakeExpectedRunClosure(FROM_HERE)));
  EXPECT_NO_FATAL_FAILURE(RunLoop().RunUntilIdle());
}

// Tests that private message loop modes are not pumped while a modal dialog is
// present.
TEST(MessagePumpMacTest, ScopedPumpMessagesAttemptWithModalDialog) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  {
    base::ScopedPumpMessagesInPrivateModes allow_private;
    // No modal window, so all modes should be pumped.
    EXPECT_EQ(kAllModesMask, allow_private.GetModeMaskForTest());
  }

  base::scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
  [alert addButtonWithTitle:@"OK"];
  base::scoped_nsobject<TestModalAlertCloser> closer(
      [[TestModalAlertCloser alloc] init]);
  [closer performSelector:@selector(runTestThenCloseAlert:)
               withObject:alert
               afterDelay:0
                  inModes:@[ NSModalPanelRunLoopMode ]];
  NSInteger result = [alert runModal];
  EXPECT_EQ(NSAlertFirstButtonReturn, result);
}

// This is a regression test for a scenario where the invalidation of the
// delayed work timer (using non-public APIs) causes a nested native run loop to
// hang. The exact root cause of the hang is unknown since it involves the
// closed-source Core Foundation runtime, but the steps needed to trigger it
// are:
//
//   1. Post a delayed task that will run some time after step #4.
//   2. Allow Chrome tasks to run in nested run loops (with
//      ScopedNestableTaskAllower).
//   3. Allow running Chrome tasks during private run loop modes (with
//      ScopedPumpMessagesInPrivateModes).
//   4. Open a pop-up menu via [NSMenu popupContextMenu]. This will start a
//      private native run loop to process menu interaction.
//   5. In a posted task, close the menu with [NSMenu cancelTracking].
//
// At this point the menu closes visually but the nested run loop (flakily)
// hangs forever in a live-lock, i.e., Chrome tasks keep executing but the
// NSMenu call in #4 never returns.
//
// The workaround is to avoid timer invalidation during nested native run loops.
//
// DANGER: As the pop-up menu captures keyboard input, the bug will make the
// machine's keyboard inoperable during the live-lock. Use a TTY-based remote
// terminal such as SSH (as opposed to Chromoting) to investigate the issue.
//
TEST(MessagePumpMacTest, DontInvalidateTimerInNativeRunLoop) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);
  NSWindow* window =
      [[[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                   styleMask:NSBorderlessWindowMask
                                     backing:NSBackingStoreBuffered
                                       defer:NO] autorelease];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Test menu"];
  [menu insertItemWithTitle:@"Dummy item"
                     action:@selector(dummy)
              keyEquivalent:@"a"
                    atIndex:0];
  NSEvent* event = [NSEvent otherEventWithType:NSApplicationDefined
                                      location:NSZeroPoint
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:nil
                                       subtype:0
                                         data1:0
                                         data2:0];

  // Post a task to open the menu. This needs to be a separate task so that
  // nested task execution can be allowed.
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](NSWindow* window, NSMenu* menu, NSEvent* event) {
                       MessageLoopCurrent::ScopedNestableTaskAllower allow;
                       ScopedPumpMessagesInPrivateModes pump_private;
                       // When the bug triggers, this call never returns.
                       [NSMenu popUpContextMenu:menu
                                      withEvent:event
                                        forView:[window contentView]];
                     },
                     window, menu, event));

  // Post another task to close the menu. The 100ms delay was determined
  // experimentally on a 2013 Mac Pro.
  RunLoop run_loop;
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](RunLoop* run_loop, NSMenu* menu) {
            [menu cancelTracking];
            run_loop->Quit();
          },
          &run_loop, menu),
      base::TimeDelta::FromMilliseconds(100));

  EXPECT_NO_FATAL_FAILURE(run_loop.Run());
}

TEST(MessagePumpMacTest, QuitWithModalWindow) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);
  NSWindow* window =
      [[[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                   styleMask:NSBorderlessWindowMask
                                     backing:NSBackingStoreBuffered
                                       defer:NO] autorelease];

  // Check that quitting the run loop while a modal window is shown applies to
  // |run_loop| rather than the internal NSApplication modal run loop.
  RunLoop run_loop;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        MessageLoopCurrent::ScopedNestableTaskAllower allow;
        ScopedPumpMessagesInPrivateModes pump_private;
        [NSApp runModalForWindow:window];
      }));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                          base::BindLambdaForTesting([&] {
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
