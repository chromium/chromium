// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_apple.h"

#import <AppKit/AppKit.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/cancelable_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

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

class MessagePumpAppleScopedRestrictNSEventMaskTest
    : public ::testing::TestWithParam<std::optional<NSEventMask>> {
 protected:
  // Returns the event mask used by ScopedRestrictNSEventMask, or NSEventMaskAny
  // if the test doesn't create one.
  NSEventMask event_mask() const { return GetParam().value_or(NSEventMaskAny); }

  // Invokes `closure` with a top-level NSApplication message loop running, and
  // a ScopedRestrictNSEventMask based on the NSEventMask parameter. Any RunLoop
  // created in `closure` will be a nested loop that's affected by the
  // ScopedRestrictNSEventMask.
  void RunWithNSAppMessageLoop(OnceClosure closure);

  // Posts an NSEvent and waits in a nested RunLoop until it's handled or a
  // timeout expires.
  void PostNSEventAndWait(NSEvent* event, bool expect_handled);

  test::TaskEnvironment& task_env() { return task_env_; }

 private:
  test::SingleThreadTaskEnvironment task_env_{
      test::SingleThreadTaskEnvironment::MainThreadType::UI};
};

void MessagePumpAppleScopedRestrictNSEventMaskTest::RunWithNSAppMessageLoop(
    OnceClosure closure) {
  std::unique_ptr<ScopedRestrictNSEventMask> scoped_restrict;
  if (GetParam().has_value()) {
    scoped_restrict =
        base::WrapUnique(new ScopedRestrictNSEventMask(GetParam().value()));
  }

  task_env_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, BindOnce([] { ASSERT_TRUE([NSApp isRunning]); })
                     .Then(std::move(closure))
                     .Then(task_env_.QuitClosure()));
  task_env_.RunUntilQuit();
}

void MessagePumpAppleScopedRestrictNSEventMaskTest::PostNSEventAndWait(
    NSEvent* event,
    bool expect_handled) {
  NSEventMask event_mask = NSEventMaskFromType([event type]);

  // Make sure no events of the given type are left over from previous tests.
  ASSERT_EQ([NSApp nextEventMatchingMask:event_mask
                               untilDate:NSDate.now
                                  inMode:NSDefaultRunLoopMode
                                 dequeue:NO],
            nil);

  RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);

  // Install a monitor that will call `handler_block` for all events with the
  // same type as `event`. It will quit `run_loop` if it sees `event`.
  __block bool handled_event = false;
  __block OnceClosure quit_closure = run_loop.QuitClosure();
  auto handler_block = ^NSEvent*(NSEvent* e) {
    // Verify that `e` is the posted event. The test should only post one
    // event of this type. Note that trying to read an NSEvent property that's
    // not supported by the specific type of event will crash with
    // NSInternalInconsistencyException, so only check common properties.
    if (handled_event) {
      ADD_FAILURE() << "Multiple events handled";
      return e;
    }
    EXPECT_EQ([e type], [event type]);
    EXPECT_EQ([e subtype], [event subtype]);
    EXPECT_EQ([e timestamp], [event timestamp]);
    if ([e type] == NSEventTypeApplicationDefined &&
        [event type] == NSEventTypeApplicationDefined) {
      // Make sure this is the test event, not one from message loop
      // internals.
      EXPECT_EQ([e data1], [event data1]);
      EXPECT_EQ([e data2], [event data2]);
    }
    if (::testing::Test::HasFailure()) {
      // Wrong event - ignore.
      return e;
    }

    handled_event = true;
    std::move(quit_closure).Run();
    return e;
  };
  id monitor = [NSEvent addLocalMonitorForEventsMatchingMask:event_mask
                                                     handler:handler_block];

  // Post `event` to the application's event queue.
  [NSApp postEvent:event atStart:NO];

  // Quit `run_loop` if it doesn't handle `event` in a reasonable time. The
  // top-level RunLoop will timeout after action_timeout() so use a shorter time
  // for the nested loop.
  OneShotTimer timer;
  timer.Start(FROM_HERE, TestTimeouts::action_timeout() / 2,
              run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_EQ(handled_event, expect_handled);

  [NSEvent removeMonitor:monitor];
}

INSTANTIATE_TEST_SUITE_P(
    Filters,
    MessagePumpAppleScopedRestrictNSEventMaskTest,
    ::testing::Values(
        // No ScopedRestrictNSEventMask installed.
        std::nullopt,
        // ScopedRestrictNSEventMask allows all NSEvents.
        NSEventMaskAny,
        // ScopedRestrictNSEventMask allows no NSEvents.
        0u,
        // ScopedRestrictNSEventMask allows only one arbitrary type of NSEvent.
        NSEventMaskPeriodic),
    // Test name formatter.
    [](const ::testing::TestParamInfo<std::optional<NSEventMask>>& info)
        -> std::string {
      if (!info.param.has_value()) {
        return "NoFilter";
      }
      switch (info.param.value()) {
        case NSEventMaskAny:
          return "NSEventMaskAny";
        case 0u:
          return "EmptyEventMask";
        case NSEventMaskPeriodic:
          return "NSEventMaskPeriodic";
      }
      return absl::StrFormat("UnknownEventMask%u", info.param.value());
    });

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

TEST_P(MessagePumpAppleScopedRestrictNSEventMaskTest, PostTask) {
  RunWithNSAppMessageLoop(BindLambdaForTesting([&] {
    // Posted tasks are seen with all event masks.
    RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        MakeExpectedRunClosure(FROM_HERE).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }));
}

TEST_P(MessagePumpAppleScopedRestrictNSEventMaskTest, PostDelayedTask) {
  RunWithNSAppMessageLoop(BindLambdaForTesting([&] {
    // Posted tasks are seen with all event masks.
    RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
    SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        MakeExpectedRunClosure(FROM_HERE).Then(run_loop.QuitClosure()),
        TestTimeouts::tiny_timeout());
    run_loop.Run();
  }));
}

TEST_P(MessagePumpAppleScopedRestrictNSEventMaskTest, MouseEvent) {
  RunWithNSAppMessageLoop(BindLambdaForTesting([&] {
    // Mouse events are seen only with NSEventMaskAny.
    NSEvent* mouse_event = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
                                              location:NSZeroPoint
                                         modifierFlags:0
                                             timestamp:0
                                          windowNumber:0
                                               context:nil
                                           eventNumber:0
                                            clickCount:0
                                              pressure:1.0];
    PostNSEventAndWait(mouse_event,
                       /*expect_handled=*/event_mask() == NSEventMaskAny);
  }));
}

TEST_P(MessagePumpAppleScopedRestrictNSEventMaskTest, PeriodicEvent) {
  RunWithNSAppMessageLoop(BindLambdaForTesting([&] {
    // Periodic events are seen with NSEventMaskAny or NSEventMaskPeriodic.
    auto periodic_event = [NSEvent otherEventWithType:NSEventTypePeriodic
                                             location:NSZeroPoint
                                        modifierFlags:0
                                            timestamp:0
                                         windowNumber:0
                                              context:nil
                                              subtype:0
                                                data1:0
                                                data2:0];
    PostNSEventAndWait(periodic_event,
                       /*expect_handled=*/event_mask() != 0);
  }));
}

TEST_P(MessagePumpAppleScopedRestrictNSEventMaskTest, ApplicationDefinedEvent) {
  RunWithNSAppMessageLoop(BindLambdaForTesting([&] {
    // Application-defined events are seen with all event masks.
    auto app_event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSZeroPoint
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                         subtype:1
                                           data1:2
                                           data2:3];
    PostNSEventAndWait(app_event, /*expect_handled=*/true);
  }));
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
