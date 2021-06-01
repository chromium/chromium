// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_H_

#include <utility>

#include "base/base_export.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
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

      // If true, native messages should be processed before executing more work
      // from the Delegate. This is an optional hint; not all message pumpls
      // implement this.
      bool yield_to_native = false;
    };

    // Executes an immediate task or a ripe delayed task. Returns information
    // about when DoWork() should be called again. If the returned NextWorkInfo
    // is_immediate(), DoWork() must be invoked again shortly. Else, DoWork()
    // must be invoked at |NextWorkInfo::delayed_run_time| or when
    // ScheduleWork() is invoked, whichever comes first. Redundant/spurious
    // invocations of DoWork() outside of those requirements are tolerated.
    // DoIdleWork() will not be called so long as this returns a NextWorkInfo
    // which is_immediate().
    virtual NextWorkInfo DoWork() = 0;

    // Called from within Run just before the message pump goes to sleep.
    // Returns true to indicate that idle work was done. Returning false means
    // the pump will now wait.
    virtual bool DoIdleWork() = 0;

    class ScopedDoWorkItem {
     public:
      ScopedDoWorkItem() : outer_(nullptr) {}

      ~ScopedDoWorkItem() {
        if (outer_)
          outer_->OnEndWorkItem();
      }

      ScopedDoWorkItem(ScopedDoWorkItem&& rhs)
          : outer_(std::exchange(rhs.outer_, nullptr)) {}
      ScopedDoWorkItem& operator=(ScopedDoWorkItem&& rhs) {
        outer_ = std::exchange(rhs.outer_, nullptr);
        return *this;
      }

     private:
      friend Delegate;

      explicit ScopedDoWorkItem(Delegate* outer) : outer_(outer) {
        outer_->OnBeginWorkItem();
      }

      Delegate* outer_;
    };

    // Called before a unit of work is executed. This allows reports
    // about individual units of work to be produced. The unit of work ends when
    // the returned ScopedDoWorkItem goes out of scope.
    // TODO(crbug.com/851163): Place calls for all platforms. Without this, some
    // state like the top-level "ThreadController active" trace event will not
    // be correct when work is performed.
    ScopedDoWorkItem BeginWorkItem() WARN_UNUSED_RESULT {
      return ScopedDoWorkItem(this);
    }

    // Called before the message pump starts waiting for work. This indicates
    // that the message pump is idle (out of application work and ideally out of
    // native work -- if it can tell).
    virtual void BeforeWait() = 0;

   private:
    // Called upon entering/exiting a ScopedDoWorkItem.
    virtual void OnBeginWorkItem() = 0;
    virtual void OnEndWorkItem() = 0;
  };

  MessagePump();
  virtual ~MessagePump();

  // The Run method is called to enter the message pump's run loop.
  //
  // Within the method, the message pump is responsible for processing native
  // messages as well as for giving cycles to the delegate periodically. The
  // message pump should take care to mix delegate callbacks with native message
  // processing so neither type of event starves the other of cycles. Each call
  // to a delegate function is considered the beginning of a new "unit of work".
  //
  // The anatomy of a typical run loop:
  //
  //   for (;;) {
  //     bool did_native_work = false;
  //     {
  //       auto scoped_do_work_item = state_->delegate->BeginWorkItem();
  //       did_native_work = DoNativeWork();
  //     }
  //     if (should_quit_)
  //       break;
  //
  //     Delegate::NextWorkInfo next_work_info = delegate->DoWork();
  //     if (should_quit_)
  //       break;
  //
  //     if (did_native_work || next_work_info.is_immediate())
  //       continue;
  //
  //     bool did_idle_work = delegate_->DoIdleWork();
  //     if (should_quit_)
  //       break;
  //
  //     if (did_idle_work)
  //       continue;
  //
  //     WaitForWork();
  //   }
  //

  // Here, DoNativeWork is some private method of the message pump that is
  // responsible for dispatching the next UI message or notifying the next IO
  // completion (for example).  WaitForWork is a private method that simply
  // blocks until there is more work of any type to do.
  //
  // Notice that the run loop cycles between calling DoNativeWork and DoWork
  // methods. This helps ensure that none of these work queues starve the
  // others. This is important for message pumps that are used to drive
  // animations, for example.
  //
  // Notice also that after each callout to foreign code, the run loop checks to
  // see if it should quit.  The Quit method is responsible for setting this
  // flag.  No further work is done once the quit flag is set.
  //
  // NOTE 1: Run may be called reentrantly from any of the callouts to foreign
  // code (internal work, DoWork, DoIdleWork). As a result, DoWork and
  // DoIdleWork must be reentrant.
  //
  // NOTE 2: Run implementations must arrange for DoWork to be invoked as
  // expected if a callout to foreign code enters a message pump outside their
  // control. For example, the MessageBox API on Windows pumps UI messages. If
  // the MessageBox API is called (indirectly) from within Run, it is expected
  // that DoWork will be invoked from within that call in response to
  // ScheduleWork or as requested by the last NextWorkInfo returned by DoWork.
  // The MessagePump::Delegate may then elect to do nested work or not depending
  // on its policy in that context. Regardless of that decision (and return
  // value of the nested DoWork() call), DoWork() will be invoked again when the
  // nested loop unwinds.
  virtual void Run(Delegate* delegate) = 0;

  // Quit immediately from the most recently entered run loop.  This method may
  // only be used on the thread that called Run.
  virtual void Quit() = 0;

  // Schedule a DoWork callback to happen reasonably soon.  Does nothing if a
  // DoWork callback is already scheduled. Once this call is made, DoWork is
  // guaranteed to be called repeatedly at least until it returns a
  // non-immediate NextWorkInfo. This call can be expensive and callers should
  // attempt not to invoke it again before a non-immediate NextWorkInfo was
  // returned from DoWork(). Thread-safe (and callers should avoid holding a
  // Lock at all cost while making this call as some platforms' priority
  // boosting features have been observed to cause the caller to get descheduled
  // : https://crbug.com/890978).
  virtual void ScheduleWork() = 0;

  // Schedule a DoWork callback to happen at the specified time, cancelling any
  // pending callback scheduled by this method. This method may only be used on
  // the thread that called Run.
  //
  // It isn't necessary to call this during normal execution, as the pump wakes
  // up as requested by the return value of DoWork().
  // TODO(crbug.com/885371): Determine if this must be called to ensure that
  // delayed tasks run when a message pump outside the control of Run is
  // entered.
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time) = 0;

  // Sets the timer slack to the specified value.
  virtual void SetTimerSlack(TimerSlack timer_slack);
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_H_
