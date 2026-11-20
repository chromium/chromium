// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_apple.h"

#import <AppKit/AppKit.h>

#include <utility>

#include "base/apple/scoped_objc_class_swizzler.h"
#include "base/auto_reset.h"
#include "base/cancelable_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface TestModalAlertCloser : NSObject
- (void)runTestThenCloseAlert:(NSAlert*)alert;
@end

@interface NSApplication (MessagePumpAppleTestSwizzle)
- (void)cr_messagePumpTest_stop:(id)sender;
- (NSEvent*)cr_messagePumpTest_nextEventMatchingMask:(NSEventMask)mask
                                           untilDate:(NSDate*)expiration
                                              inMode:(NSRunLoopMode)mode
                                             dequeue:(BOOL)dequeue;
@end

namespace base {
namespace {

class ScopedNSAppObserver {
 public:
  explicit ScopedNSAppObserver(OnceClosure on_first_next_event = {})
      : on_first_next_event_(std::move(on_first_next_event)),
        reset_active_(&g_active_observer_, this),
        stop_swizzler_([NSApplication class],
                       @selector(stop:),
                       @selector(cr_messagePumpTest_stop:)),
        next_event_swizzler_(
            [NSApplication class],
            @selector(nextEventMatchingMask:untilDate:inMode:dequeue:),
            @selector(cr_messagePumpTest_nextEventMatchingMask:untilDate:inMode:
                      dequeue:)) {
    [NSApplication sharedApplication];
  }

  int stop_calls() const { return stop_calls_; }
  int next_event_calls() const { return next_event_calls_; }
  static ScopedNSAppObserver* active() { return g_active_observer_; }

  void HandleStop(id receiver, SEL selector, id sender) {
    ++stop_calls_;
    stop_swizzler_.InvokeOriginal<void>(receiver, selector, sender);
  }

  NSEvent* HandleNextEvent(id receiver,
                           SEL selector,
                           NSEventMask mask,
                           NSDate* expiration,
                           NSRunLoopMode mode,
                           BOOL dequeue) {
    if (++next_event_calls_ == 1 && on_first_next_event_) {
      std::move(on_first_next_event_).Run();
    }
    return next_event_swizzler_.InvokeOriginal<NSEvent*>(
        receiver, selector, mask, expiration, mode, dequeue);
  }

 private:
  static ScopedNSAppObserver* g_active_observer_;
  OnceClosure on_first_next_event_;
  AutoReset<ScopedNSAppObserver*> reset_active_;
  int stop_calls_ = 0;
  int next_event_calls_ = 0;
  apple::ScopedObjCClassSwizzler stop_swizzler_;
  apple::ScopedObjCClassSwizzler next_event_swizzler_;
};

ScopedNSAppObserver* ScopedNSAppObserver::g_active_observer_ = nullptr;

}  // namespace
}  // namespace base

@implementation NSApplication (MessagePumpAppleTestSwizzle)
- (void)cr_messagePumpTest_stop:(id)sender {
  base::ScopedNSAppObserver::active()->HandleStop(self, _cmd, sender);
}
- (NSEvent*)cr_messagePumpTest_nextEventMatchingMask:(NSEventMask)mask
                                           untilDate:(NSDate*)expiration
                                              inMode:(NSRunLoopMode)mode
                                             dequeue:(BOOL)dequeue {
  return base::ScopedNSAppObserver::active()->HandleNextEvent(
      self, _cmd, mask, expiration, mode, dequeue);
}
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

// Verifies pre-`-[NSApplication run]` and post-`-[NSApplication
// finishLaunching]` task drains, continuation of remaining work in the main
// `CFRunLoop`, and one-shot budget consumption.
TEST(MessagePumpAppleTest, StartupDrainBeforeAndAfterFinishLaunching) {
  test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kMacPumpTasksBeforeFinishLaunching,
      {{kMacPumpBeforeFinishLaunchingMaxMs.name, "30"},
       {kMacPumpAfterFinishLaunchingMaxMs.name, "30"}});
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI,
      test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME);

  int tasks_executed = 0;
  int tasks_after_pre_drain = -1;
  int tasks_after_post_drain = -1;
  ScopedNSAppObserver observer(BindLambdaForTesting(
      [&tasks_executed, &tasks_after_pre_drain, &tasks_after_post_drain] {
        EXPECT_TRUE(NSApp.running);
        tasks_after_pre_drain = tasks_executed;
        message_pump_apple::DrainPostFinishLaunchingTasks();
        tasks_after_post_drain = tasks_executed;
      }));

  RunLoop run_loop;
  auto task_runner = SingleThreadTaskRunner::GetCurrentDefault();
  EXPECT_FALSE(message_pump_apple::IsDrainingStartupTasks());
  // The first two tasks (10ms + 25ms) consume the 30ms pre-launch budget.
  task_runner->PostTask(
      FROM_HERE, BindLambdaForTesting([&tasks_executed, &task_environment] {
        EXPECT_FALSE(NSApp.running);
        EXPECT_TRUE(message_pump_apple::IsDrainingStartupTasks());
        ++tasks_executed;
        task_environment.AdvanceClock(Milliseconds(10));
      }));
  task_runner->PostTask(
      FROM_HERE, BindLambdaForTesting([&tasks_executed, &task_environment] {
        EXPECT_FALSE(NSApp.running);
        EXPECT_TRUE(message_pump_apple::IsDrainingStartupTasks());
        ++tasks_executed;
        task_environment.AdvanceClock(Milliseconds(25));
      }));
  task_runner->PostTask(
      FROM_HERE, BindLambdaForTesting([&tasks_executed, &task_runner, &run_loop,
                                       &task_environment] {
        EXPECT_TRUE(NSApp.running);
        EXPECT_TRUE(message_pump_apple::IsDrainingStartupTasks());
        ++tasks_executed;
        task_runner->PostTask(
            FROM_HERE, BindLambdaForTesting([&tasks_executed, &run_loop] {
              EXPECT_TRUE(NSApp.running);
              EXPECT_FALSE(message_pump_apple::IsDrainingStartupTasks());
              ++tasks_executed;
              // Calling `Quit()` a second time after `-[NSApplication stop:]`
              // clears `NSApp.running` must not leave `quit_pending_` armed for
              // subsequent `RunLoop`s.
              run_loop.Quit();
              run_loop.Quit();
            }));
        task_environment.AdvanceClock(Milliseconds(35));
      }));

  run_loop.Run();
  EXPECT_FALSE(message_pump_apple::IsDrainingStartupTasks());
  EXPECT_EQ(tasks_after_pre_drain, 2);
  EXPECT_EQ(tasks_after_post_drain, 3);
  EXPECT_EQ(tasks_executed, 4);

  // Subsequent RunLoops on the same pump must enter `-[NSApp run]` directly.
  RunLoop second_run_loop;
  bool ran_inside_nsapp = false;
  task_runner->PostTask(
      FROM_HERE, BindLambdaForTesting([&ran_inside_nsapp, &second_run_loop] {
        ran_inside_nsapp = NSApp.running;
        second_run_loop.Quit();
      }));
  second_run_loop.Run();
  EXPECT_TRUE(ran_inside_nsapp);
}

// Verifies that `RunLoop::Quit()` inside a pre-launch task exits `Run()`
// cleanly without calling `-[NSApplication run]` or `-[NSApplication stop:]`.
TEST(MessagePumpAppleTest, PreLaunchDrainQuitExitsWithoutRunningNSApp) {
  test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kMacPumpTasksBeforeFinishLaunching,
      {{kMacPumpBeforeFinishLaunchingMaxMs.name, "5000"},
       {kMacPumpAfterFinishLaunchingMaxMs.name, "5000"}});
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  ScopedNSAppObserver observer;

  RunLoop run_loop;
  bool first_task_ran = false;
  bool second_task_ran = false;
  auto task_runner = SingleThreadTaskRunner::GetCurrentDefault();
  task_runner->PostTask(FROM_HERE,
                        BindLambdaForTesting([&first_task_ran, &run_loop] {
                          EXPECT_FALSE(NSApp.running);
                          first_task_ran = true;
                          run_loop.Quit();
                        }));
  task_runner->PostTask(FROM_HERE, BindLambdaForTesting([&second_task_ran] {
                          second_task_ran = true;
                        }));

  run_loop.Run();
  EXPECT_TRUE(first_task_ran);
  EXPECT_FALSE(second_task_ran);
  EXPECT_EQ(observer.next_event_calls(), 0);
  EXPECT_EQ(observer.stop_calls(), 0);
}

// Verifies that a nested `RunLoop` started from a pre-launch drained task runs
// and quits normally without entering `-[NSApplication run]` prematurely.
TEST(MessagePumpAppleTest, NestedRunLoopDuringPreLaunchDrain) {
  test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kMacPumpTasksBeforeFinishLaunching,
      {{kMacPumpBeforeFinishLaunchingMaxMs.name, "5000"},
       {kMacPumpAfterFinishLaunchingMaxMs.name, "0"}});
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  RunLoop run_loop;
  bool nested_task_ran = false;
  bool nested_run_returned = false;
  auto task_runner = SingleThreadTaskRunner::GetCurrentDefault();
  task_runner->PostTask(
      FROM_HERE, BindLambdaForTesting([&task_runner, &nested_task_ran,
                                       &nested_run_returned, &run_loop] {
        EXPECT_FALSE(NSApp.running);
        EXPECT_TRUE(message_pump_apple::IsDrainingStartupTasks());
        RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);
        task_runner->PostTask(
            FROM_HERE,
            BindLambdaForTesting([&nested_task_ran, &nested_run_loop] {
              EXPECT_FALSE(NSApp.running);
              EXPECT_FALSE(message_pump_apple::IsDrainingStartupTasks());
              nested_task_ran = true;
              nested_run_loop.Quit();
            }));
        nested_run_loop.Run();
        EXPECT_FALSE(NSApp.running);
        EXPECT_TRUE(message_pump_apple::IsDrainingStartupTasks());
        nested_run_returned = true;
        run_loop.Quit();
      }));

  run_loop.Run();
  EXPECT_TRUE(nested_task_ran);
  EXPECT_TRUE(nested_run_returned);
}

// Verifies that `RunLoop::Quit()` during `DrainPostFinishLaunchingTasks()`
// stops draining immediately, even if the quitting task posts more work, and
// defers `-[NSApplication stop:]` until `-[NSApplication run]` enters its main
// `CFRunLoop`.
TEST(MessagePumpAppleTest,
     PostFinishLaunchingDrainQuitDeferredUntilRunLoopEntry) {
  test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kMacPumpTasksBeforeFinishLaunching,
      {{kMacPumpBeforeFinishLaunchingMaxMs.name, "0"},
       {kMacPumpAfterFinishLaunchingMaxMs.name, "5000"}});
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  int stop_calls_during_post_drain = -1;
  ScopedNSAppObserver observer(
      BindLambdaForTesting([&stop_calls_during_post_drain] {
        message_pump_apple::DrainPostFinishLaunchingTasks();
        stop_calls_during_post_drain =
            ScopedNSAppObserver::active()->stop_calls();
      }));

  RunLoop run_loop;
  RunLoop second_run_loop;
  bool first_task_ran = false;
  bool posted_task_ran = false;
  auto task_runner = SingleThreadTaskRunner::GetCurrentDefault();
  task_runner->PostTask(
      FROM_HERE, BindLambdaForTesting([&first_task_ran, &run_loop, &task_runner,
                                       &posted_task_ran, &second_run_loop] {
        EXPECT_TRUE(NSApp.running);
        first_task_ran = true;
        run_loop.Quit();
        // Posting while the queue is empty makes `DoWork()` report immediate
        // work despite the pending quit.
        task_runner->PostTask(
            FROM_HERE,
            BindLambdaForTesting([&posted_task_ran, &second_run_loop] {
              posted_task_ran = true;
              second_run_loop.Quit();
            }));
      }));

  run_loop.Run();
  EXPECT_TRUE(first_task_ran);
  EXPECT_FALSE(posted_task_ran);
  EXPECT_EQ(stop_calls_during_post_drain, 0);
  EXPECT_EQ(observer.stop_calls(), 1);

  second_run_loop.Run();
  EXPECT_TRUE(posted_task_ran);
}

// Verifies that `RunLoop::QuitWhenIdle()` during startup drains neither stops
// remaining immediate tasks nor invokes `DoIdleWork()` until
// `-[NSApplication run]` enters the main `CFRunLoop`.
TEST(MessagePumpAppleTest, StartupDrainQuitWhenIdleWaitsForMainRunLoop) {
  test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kMacPumpTasksBeforeFinishLaunching,
      {{kMacPumpBeforeFinishLaunchingMaxMs.name, "30"},
       {kMacPumpAfterFinishLaunchingMaxMs.name, "5000"}});
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI,
      test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME);

  int tasks_executed = 0;
  int tasks_after_post_drain = -1;
  int stops_after_post_drain = -1;
  bool ran_in_main_loop = false;
  ScopedNSAppObserver observer(
      BindLambdaForTesting([&tasks_after_post_drain, &tasks_executed,
                            &stops_after_post_drain, &ran_in_main_loop] {
        message_pump_apple::DrainPostFinishLaunchingTasks();
        tasks_after_post_drain = tasks_executed;
        stops_after_post_drain = ScopedNSAppObserver::active()->stop_calls();
        // If the post drain had run idle work, the deferred quit would stop the
        // main loop before this task runs.
        SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, BindLambdaForTesting(
                           [&ran_in_main_loop] { ran_in_main_loop = true; }));
      }));

  RunLoop run_loop;
  auto task_runner = SingleThreadTaskRunner::GetCurrentDefault();
  task_runner->PostTask(
      FROM_HERE,
      BindLambdaForTesting([&tasks_executed, &run_loop, &task_environment] {
        EXPECT_FALSE(NSApp.running);
        ++tasks_executed;
        run_loop.QuitWhenIdle();
        task_environment.AdvanceClock(Milliseconds(35));
      }));
  task_runner->PostTask(
      FROM_HERE, BindLambdaForTesting([&tasks_executed, &task_runner] {
        EXPECT_TRUE(NSApp.running);
        ++tasks_executed;
        task_runner->PostTask(FROM_HERE,
                              BindLambdaForTesting([&tasks_executed] {
                                EXPECT_TRUE(NSApp.running);
                                ++tasks_executed;
                              }));
      }));

  run_loop.Run();
  EXPECT_EQ(tasks_after_post_drain, 3);
  EXPECT_EQ(stops_after_post_drain, 0);
  EXPECT_TRUE(ran_in_main_loop);
  EXPECT_EQ(observer.stop_calls(), 1);
}

// Verifies that both startup drains are skipped when
// `kMacPumpTasksBeforeFinishLaunching` is disabled.
TEST(MessagePumpAppleTest, StartupDrainDisabledWhenFeatureDisabled) {
  test::ScopedFeatureList disabled_list;
  disabled_list.InitAndDisableFeature(kMacPumpTasksBeforeFinishLaunching);
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  int tasks_executed = 0;
  int tasks_after_post_drain = -1;
  ScopedNSAppObserver observer(
      BindLambdaForTesting([&tasks_executed, &tasks_after_post_drain] {
        message_pump_apple::DrainPostFinishLaunchingTasks();
        tasks_after_post_drain = tasks_executed;
      }));

  RunLoop run_loop;
  bool ran_inside_nsapp = false;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindLambdaForTesting([&tasks_executed, &ran_inside_nsapp, &run_loop] {
        EXPECT_FALSE(message_pump_apple::IsDrainingStartupTasks());
        ++tasks_executed;
        ran_inside_nsapp = NSApp.running;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(tasks_after_post_drain, 0);
  EXPECT_EQ(tasks_executed, 1);
  EXPECT_TRUE(ran_inside_nsapp);
}

}  // namespace base
