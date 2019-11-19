// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_H_

#include "base/base_export.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/message_loop/timer_slack.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {

class TimeTicks;

class BASE_EXPORT MessagePump {
 public:
  using MessagePumpFactory = std::unique_ptr<MessagePump>();
  // Uses the given base::MessagePumpFactory to override the default MessagePump
  // implementation for 'MessagePumpType::UI'. May only be called once.
  static void OverrideMessagePumpForUIFactory(MessagePumpFactory* factory);

  // Returns true if the MessagePumpForUI has been overidden.
  static bool IsMessagePumpForUIFactoryOveridden();

  // Creates the default MessagePump based on |type|. Caller owns return value.
  static std::unique_ptr<MessagePump> Create(MessagePumpType type);

  // Please see the comments above the Run method for an illustration of how
  // these delegate methods are used.
  class BASE_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    // Called before work performed internal to the message pump is executed,
    // including waiting for a wake up. Currently only called on Windows.
    // TODO(wittman): Implement for other platforms.
    virtual void BeforeDoInternalWork() = 0;

    struct NextWorkInfo {
      // Helper to extract a TimeDelta for pumps that need a
      // timeout-till-next-task.
      TimeDelta remaining_delay() const {
        DCHECK(!delayed_run_time.is_null() && !delayed_run_time.is_max());
        DCHECK_GE(TimeTicks::Now(), recent_now);
        return delayed_run_time - recent_now;
      }

      // Helper to verify if the next task is ready right away.
      bool is_immediate() const { return delayed_run_time.is_null(); }

      // The next PendingTask's |delayed_run_time|. is_null() if there's extra
      // work to run immediately. is_max() if there are no more immediate nor
      // delayed tasks.
      TimeTicks delayed_run_time;

      // A recent view of TimeTicks::Now(). Only valid if |next_task_run_time|
      // isn't null nor max. MessagePump impls should use remaining_delay()
      // instead of resampling Now() if they wish to sleep for a TimeDelta.
      TimeTicks recent_now;
    };

    // The latest model of MessagePumps will invoke this instead of
    // DoWork()/DoDelayedWork(). Executes an immediate task or a ripe delayed
    // task. Returns a struct which indicates |delayed_run_time|. DoSomeWork()
    // will be invoked again shortly if is_immediate(); it will be invoked after
    // |delayed_run_time| (or ScheduleWork()) if there isn't immediate work and
    // |!delayed_run_time.is_max()|; and it will not be invoked again until
    // ScheduleWork() otherwise. Redundant/spurious invocations outside of those
    // guarantees are not impossible however. DoIdleWork() will not be called so
    // long as this returns a NextWorkInfo which is_immediate(). See design doc
    // for details :
    // https://docs.google.com/document/d/1no1JMli6F1r8gTF9KDIOvoWkUUZcXDktPf4A1IXYc3M/edit#
    virtual NextWorkInfo DoSomeWork() = 0;

    // Called from within Run in response to ScheduleWork or when the message
    // pump would otherwise call DoDelayedWork.  Returns true to indicate that
    // work was done.  DoDelayedWork will still be called if DoWork returns
    // true, but DoIdleWork will not.
    // Used in conjunction with DoDelayedWork() by old MessagePumps.
    // TODO(gab): Migrate such pumps to DoSomeWork().
    virtual bool DoWork() = 0;

    // Called from within Run in response to ScheduleDelayedWork or when the
    // message pump would otherwise sleep waiting for more work.  Returns true
    // to indicate that delayed work was done.  DoIdleWork will not be called
    // if DoDelayedWork returns true.  Upon return |next_delayed_work_time|
    // indicates the time when DoDelayedWork should be called again.  If
    // |next_delayed_work_time| is null (per Time::is_null), then the queue of
    // future delayed work (timer events) is currently empty, and no additional
    // calls to this function need to be scheduled.
    // Used in conjunction with DoWork() by old MessagePumps.
    // TODO(gab): Migrate such pumps to DoSomeWork().
    virtual bool DoDelayedWork(TimeTicks* next_delayed_work_time) = 0;

    // Called from within Run just before the message pump goes to sleep.
    // Returns true to indicate that idle work was done. Returning false means
    // the pump will now wait.
    virtual bool DoIdleWork() = 0;
  };

  MessagePump();
  virtual ~MessagePump();

  // The Run method is called to enter the message pump's run loop.
  //
  // Within the method, the message pump is responsible for processing native
  // messages as well as for giving cycles to the delegate periodically.  The
  // message pump should take care to mix delegate callbacks with native
  // message processing so neither type of event starves the other of cycles.
  //
  // The anatomy of a typical run loop:
  //
  //   for (;;) {
  //     bool did_work = DoInternalWork();
  //     if (should_quit_)
  //       break;
  //
  //     did_work |= delegate_->DoWork();
  //     if (should_quit_)
  //       break;
  //
  //     TimeTicks next_time;
  //     did_work |= delegate_->DoDelayedWork(&next_time);
  //     if (should_quit_)
  //       break;
  //
  //     if (did_work)
  //       continue;
  //
  //     did_work = delegate_->DoIdleWork();
  //     if (should_quit_)
  //       break;
  //
  //     if (did_work)
  //       continue;
  //
  //     WaitForWork();
  //   }
  //
  // Here, DoInternalWork is some private method of the message pump that is
  // responsible for dispatching the next UI message or notifying the next IO
  // completion (for example).  WaitForWork is a private method that simply
  // blocks until there is more work of any type to do.
  //
  // Notice that the run loop cycles between calling DoInternalWork, DoWork,
  // and DoDelayedWork methods.  This helps ensure that none of these work
  // queues starve the others.  This is important for message pumps that are
  // used to drive animations, for example.
  //
  // Notice also that after each callout to foreign code, the run loop checks
  // to see if it should quit.  The Quit method is responsible for setting this
  // flag.  No further work is done once the quit flag is set.
  //
  // NOTE: Care must be taken to handle Run being called again from within any
  // of the callouts to foreign code.  Native message pumps may also need to
  // deal with other native message pumps being run outside their control
  // (e.g., the MessageBox API on Windows pumps UI messages!).  To be specific,
  // the callouts (DoWork and DoDelayedWork) MUST still be provided even in
  // nested sub-loops that are "seemingly" outside the control of this message
  // pump.  DoWork in particular must never be starved for time slices unless
  // it returns false (meaning it has run out of things to do).
  //
  virtual void Run(Delegate* delegate) = 0;

  // Quit immediately from the most recently entered run loop.  This method may
  // only be used on the thread that called Run.
  virtual void Quit() = 0;

  // Schedule a DoSomeWork callback to happen reasonably soon.  Does nothing if
  // a DoSomeWork callback is already scheduled. Once this call is made,
  // DoSomeWork is guaranteed to be called repeatedly at least until it returns
  // a non-immediate NextWorkInfo (or, if this pump wasn't yet migrated,
  // DoWork() will be called until it returns false). This call can be expensive
  // and callers should attempt not to invoke it again before a non-immediate
  // NextWorkInfo was returned from DoSomeWork(). Thread-safe (and callers
  // should avoid holding a Lock at all cost while making this call as some
  // platforms' priority boosting features have been observed to cause the
  // caller to get descheduled : https://crbug.com/890978).
  virtual void ScheduleWork() = 0;

  // Schedule a DoDelayedWork callback to happen at the specified time,
  // cancelling any pending DoDelayedWork callback. This method may only be used
  // on the thread that called Run.
  //
  // This is mostly a no-op in the DoSomeWork() world but must still be invoked
  // when the new |delayed_work_time| is sooner than the last one returned from
  // DoSomeWork(). TODO(gab): Clarify this API once all pumps have been
  // migrated.
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time) = 0;

  // Sets the timer slack to the specified value.
  virtual void SetTimerSlack(TimerSlack timer_slack);
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_H_
