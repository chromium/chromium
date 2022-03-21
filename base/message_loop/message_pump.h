// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_H_

#include <memory>
#include <utility>

#include "base/base_export.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_pump_type.h"
#include "base/message_loop/timer_slack.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"

/**
 * @brief MessagePump.Run()流程：
 * 1. delegate->DoWork()，
 * 2. delegate->DoDelayedWork(&delayed_work_time_)
 * 3. delegate->DoIdleWork()。
 *
 * // 参考:
 * https://blog.csdn.net/milado_nju/article/details/8539795
 * https://www.cswamp.com/post/40
 * https://zhuanlan.zhihu.com/p/416583509
 * https://www.shuzhiduo.com/A/8Bz8vgpk5x/
 * https://www.shuzhiduo.com/A/6pdDBwgkJw/
 *
 * 消息泵（消息循环）
 * 类MessagePump: 一个抽象出来的基类，可以用来处理第二和第三种消息类型。对于每个平台，
 * 它们有不同的 MessagePump 的子类来对应，这些子类被包含在 MessageLoopForUI 和
 * MessageLoopForIO 类中。
 */

namespace base {

class TimeTicks;

class BASE_EXPORT MessagePump {
 public:
  using MessagePumpFactory = std::unique_ptr<MessagePump>();
  // Uses the given base::MessagePumpFactory to override the default MessagePump
  // implementation for 'MessagePumpType::UI'. May only be called once.
  // 使用给定的 base::MessagePumpFactory 覆盖 “MessagePumpType::UI” 的默认
  // MessagePump 实现。 只能调用一次。
  static void OverrideMessagePumpForUIFactory(MessagePumpFactory* factory);

  // Returns true if the MessagePumpForUI has been overidden.
  static bool IsMessagePumpForUIFactoryOveridden();

  // Creates the default MessagePump based on |type|. Caller owns return value.
  // 创建一个消息循环对象
  // MessagePump::Create 创建消息循环后，线程入口函数，也就是 Mac OS 的 pthread_create
  // 和 Windows 的 CreateThread 接收的线程入口函数 ThreadFunc，开始运行消息循环，消息循
  // 环源码如下：
  static std::unique_ptr<MessagePump> Create(MessagePumpType type);

  // Please see the comments above the Run method for an illustration of how
  // these delegate methods are used.
  // 有关如何使用这些委托方法的说明，请参阅 Run 方法上方的注释。
  class BASE_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    struct NextWorkInfo {
      // Helper to extract a TimeDelta for pumps that need a
      // timeout-till-next-task.
      // Helper 为需要 a 的泵提取 TimeDelta 超时直到下一个任务。
      TimeDelta remaining_delay() const {
        DCHECK(!delayed_run_time.is_null() && !delayed_run_time.is_max());
        DCHECK_GE(TimeTicks::Now(), recent_now);
        return delayed_run_time - recent_now;
      }

      // Helper to verify if the next task is ready right away.
      // 帮助验证下一个任务是否立即准备就绪。
      bool is_immediate() const {
        return delayed_run_time.is_null();
      }

      // The next PendingTask's |delayed_run_time|. is_null() if there's extra
      // work to run immediately. is_max() if there are no more immediate nor
      // delayed tasks.
      // 下一个 PendingTask 的 |delayed_run_time|。
      // is_null() 如果有额外的工作要立即运行。 is_max() 如果没有更多即时或延迟任务。
      TimeTicks delayed_run_time;

      // A recent view of TimeTicks::Now(). Only valid if |next_task_run_time|
      // isn't null nor max. MessagePump impls should use remaining_delay()
      // instead of resampling Now() if they wish to sleep for a TimeDelta.
      // TimeTicks::Now() 的最新视图。 仅在 |next_task_run_time| 时有效 不为空也不
      // 为最大值。 如果 MessagePump impls 想要休眠一个 TimeDelta，则应该使用
      // remaining_delay() 而不是重新采样 Now()。
      TimeTicks recent_now;

      // If true, native messages should be processed before executing more work
      // from the Delegate. This is an optional hint; not all message pumpls
      // implement this.
      // 如果为 true，则应先处理本机消息，然后再从 Delegate 执行更多工作。 这是一个可选提示；
      // 并非所有消息泵都实现了这一点。
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
    // 执行即时任务或成熟的延迟任务。返回有关何时应再次调用 DoWork() 的信息。
    // 如果返回的 NextWorkInfo is_immediate()，则必须很快再次调用 DoWork()。
    // 否则，必须在 |NextWorkInfo::delayed_run_time| 调用 DoWork() 或调用
    // ScheduleWork() 时，以先到者为准。 允许在这些要求之外对 DoWork() 进行冗余/虚假调用。
    // 只要返回一个 NextWorkInfo 即 is_immediate()，就不会调用 DoIdleWork()。
    virtual NextWorkInfo DoWork() = 0;

    // Called from within Run just before the message pump goes to sleep.
    // Returns true to indicate that idle work was done. Returning false means
    // the pump will now wait.
    // 在消息泵进入睡眠状态之前从 Run 内部调用。 返回 true 表示空闲工作已完成。 返回
    // false 意味着泵现在将等待。
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
    // 在执行工作单元之前调用。 这允许生成有关单个工作单元的报告。
    // 当返回的 ScopedDoWorkItem 超出范围时，工作单元结束。
    // TODO(crbug.com/851163)：为所有平台发出呼叫。 没有这个，在执行工作时，
    // 像顶级“ThreadController active”跟踪事件这样的状态将不正确。
    ScopedDoWorkItem BeginWorkItem() WARN_UNUSED_RESULT {
      return ScopedDoWorkItem(this);
    }

    // Called before the message pump starts waiting for work. This indicates
    // that the message pump is idle (out of application work and ideally out of
    // native work -- if it can tell).
    // 在消息泵开始等待工作之前调用。 这表明消息泵处于空闲状态（没有应用程序工作，理想情况下
    // 没有本地工作——如果可以的话）。
    virtual void BeforeWait() = 0;

   private:
    // Called upon entering/exiting a ScopedDoWorkItem.
    // 在进入/退出 ScopedDoWorkItem 时调用。
    virtual void OnBeginWorkItem() = 0;
    virtual void OnEndWorkItem() = 0;
  };

  MessagePump();
  virtual ~MessagePump();

  // The Run method is called to enter the message pump's run loop.
  // 调用 Run 方法进入消息泵的运行循环。
  //
  // Within the method, the message pump is responsible for processing native
  // messages as well as for giving cycles to the delegate periodically. The
  // message pump should take care to mix delegate callbacks with native message
  // processing so neither type of event starves the other of cycles. Each call
  // to a delegate function is considered the beginning of a new "unit of work".
  // 在该方法中，消息泵负责处理本地消息以及定期向委托提供周期。
  // 消息泵应该注意将委托回调与本机消息处理混合在一起，这样任何一种类型的事件都不会饿死另一
  // 个循环。 对委托函数的每次调用都被视为新“工作单元”的开始。
  //
  // The anatomy of a typical run loop:
  // 典型运行循环的剖析：
  //
  //   for (;;) {
  //     bool did_native_work = false;
  //     {
  //       auto scoped_do_work_item = state_->delegate->BeginWorkItem();
  //       did_native_work = DoNativeWork(); // 负责调度下一个 UI 消息或通知下一个 IO
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
  //     WaitForWork(); // 阻塞，直到有更多任何类型的工作要做。
  //   }
  //

  // Here, DoNativeWork is some private method of the message pump that is
  // responsible for dispatching the next UI message or notifying the next IO
  // completion (for example).  WaitForWork is a private method that simply
  // blocks until there is more work of any type to do.
  // 这里，DoNativeWork 是消息泵的一些私有方法，负责调度下一个 UI 消息或通知下一个 IO
  // 完成（例如）。 WaitForWork 是一个私有方法，它只是阻塞，直到有更多任何类型的工作要做。
  //
  // Notice that the run loop cycles between calling DoNativeWork and DoWork
  // methods. This helps ensure that none of these work queues starve the
  // others. This is important for message pumps that are used to drive
  // animations, for example.
  // 请注意，运行循环在调用 DoNativeWork 和 DoWork 方法之间循环。
  // 这有助于确保这些工作队列中的任何一个都不会使其他工作队列挨饿。
  // 例如，这对于用于驱动动画的消息泵很重要。
  //
  // Notice also that after each callout to foreign code, the run loop checks to
  // see if it should quit.  The Quit method is responsible for setting this
  // flag.  No further work is done once the quit flag is set.
  // 还要注意，在每次调用外部代码之后，运行循环都会检查它是否应该退出。
  // Quit 方法负责设置此标志。 一旦设置了退出标志，就不再进行任何工作。
  //
  // NOTE 1: Run may be called reentrantly from any of the callouts to foreign
  // code (internal work, DoWork, DoIdleWork). As a result, DoWork and
  // DoIdleWork must be reentrant.
  // 注 1：可以从任何外部代码（内部工作、DoWork、DoIdleWork）的标注中重入调用 Run。
  // 因此，DoWork 和 DoIdleWork 必须是可重入的。
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
  // 注意 2：如果外部代码的调出进入其控制之外的消息泵，则运行实现必须安排按预期调用 DoWork。
  // 例如，Windows 上的 MessageBox API 会抽取 UI 消息。 如果从 Run 中（间接）调用
  // MessageBox API，则预计 DoWork 将从该调用中调用，以响应 ScheduleWork 或 DoWork
  // 返回的最后一个 NextWorkInfo 的请求。 然后 MessagePump::Delegate 可以根据其在该上
  // 下文中的策略选择是否执行嵌套工作。 无论该决定如何（以及嵌套 DoWork() 调用的返回值），
  // 当嵌套循环展开时，将再次调用 DoWork()。
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
