// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_apple.h"

#import <AppKit/AppKit.h>

#include <utility>

#include "base/cancelable_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface TestModalAlertCloser : NSObject
- (void)runTestThenCloseAlert:(NSAlert*)alert;
@end

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

// Regression test for a crash where a nested run loop started from a
// PreWaitObserver callback (e.g. DoIdleWork) caused an imbalance in the work
// item scope stack. The crash happened because the nested loop's Entry/Exit
// observers pushed/popped scopes correctly, but the nested loop's
// AfterWaitObserver (if triggered) would push a scope that wasn't popped by a
// corresponding PreWaitObserver (because the outer PreWaitObserver popped the
// outer scope, not the nested one). The fix involves tracking which nesting
// levels have actually slept.
TEST(MessagePumpAppleTest, DirectNestedRunInIdleWork) {
  auto pump = MessagePump::Create(MessagePumpType::UI);

  class NestedDelegate : public MessagePump::Delegate {
   public:
    explicit NestedDelegate(MessagePump* pump) : pump_(pump) {}

    void BeforeWait() override {}
    void BeginNativeWorkBeforeDoWork() override {}
    int RunDepth() override { return is_nested_ ? 2 : 1; }
    void OnBeginWorkItem() override {}
    void OnEndWorkItem(int) override {}

    NextWorkInfo DoWork() override {
      if (is_nested_) {
        pump_->Quit();
      }
      return NextWorkInfo{TimeTicks::Max()};
    }

    void DoIdleWork() override {
      if (!was_nested_) {
        was_nested_ = true;
        is_nested_ = true;
        pump_->Run(this);
        is_nested_ = false;
        pump_->Quit();
      }
    }

    raw_ptr<MessagePump> pump_;
    bool was_nested_ = false;
    bool is_nested_ = false;
  } delegate(pump.get());

  pump->Run(&delegate);

  EXPECT_TRUE(delegate.was_nested_);
}

// Regression test for crbug.com/571794773: when a nested RunLoop is started
// from inside -[NSApp sendEvent:] (outside CFRunLoopRunSpecific) and quit from
// a posted task that also schedules follow-up work for the outer RunLoop, the
// outer RunLoop must not lose the work_source_ wakeup while the nested
// MessagePumpNSApplication::DoRun() drains the dummy quit event.
TEST(MessagePumpAppleTest, QuitNestedRunLoopFromNativeEventSchedulesOuterWork) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  constexpr short kStartNestedLoopSubtype = 1234;
  RunLoop outer_run_loop;
  bool outer_continuation_ran = false;
  RunLoop* outer_run_loop_ptr = &outer_run_loop;
  bool* outer_continuation_ran_ptr = &outer_continuation_ran;

  id monitor = [NSEvent
      addLocalMonitorForEventsMatchingMask:NSEventMaskApplicationDefined
                                   handler:^NSEvent*(NSEvent* event) {
                                     if (event.subtype !=
                                         kStartNestedLoopSubtype) {
                                       return event;
                                     }
                                     RunLoop nested_run_loop;
                                     SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                                         FROM_HERE, BindLambdaForTesting([&] {
                                           nested_run_loop.Quit();
                                           SingleThreadTaskRunner::GetCurrentDefault()
                                               ->PostTask(
                                                   FROM_HERE,
                                                   BindLambdaForTesting(
                                                       [outer_continuation_ran_ptr,
                                                        outer_run_loop_ptr] {
                                                         *outer_continuation_ran_ptr =
                                                             true;
                                                         outer_run_loop_ptr
                                                             ->Quit();
                                                       }));
                                         }));
                                     nested_run_loop.Run();
                                     return event;
                                   }];

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        [NSApp
            postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                         location:NSZeroPoint
                                    modifierFlags:0
                                        timestamp:0
                                     windowNumber:0
                                          context:nil
                                          subtype:kStartNestedLoopSubtype
                                            data1:0
                                            data2:0]
              atStart:NO];
      }));

  outer_run_loop.Run();
  [NSEvent removeMonitor:monitor];

  EXPECT_TRUE(outer_continuation_ran);
}

}  // namespace base
