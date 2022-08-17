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

// 消息泵，是提供消息循环的动力所在，重要程度就像一架飞机的心脏 “发动机” 一样，内部与OS紧
// 密结合，提供高效的消息循环执行能力。
// Chromium中需要处理三种类型的消息：chromium自定义任务，Socket，文件IO操作。
// 参考：https://lybvinci.github.io/post/chromium-mo-xing-xian-cheng-jie-shao/
//
// 如何等待自定义任务？
// 假设现在 MesasgePump 中没有任务和消息需要处理，就需要等待自定义任务到来。不能盲
// 目等待。chromium非常巧妙，他在MessagePumpLibEvent为例：在Linux上创建一个管
// 道(pipe)，等待读取这个管道的内容，当有新的自定义任务到来的时候，就写入一个字节到
// 管道中，从而MessagePumpLibEvent被唤醒，简单直接。

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

// http://r12f.com/posts/learning-chrome-1-threading-model-and-messageloop/
// chromium 消息循环的有点：
// 1. 减少锁的请求
// 一般我们在实现任务队列时，会简单的将任务队列加锁，以避免多线程的访问，但是这样每取一次任务，
// 都要访问一次锁。一旦任务变多，效率上必然成问题。
// Chromium 为了实现上尽可能少的使用锁，在接收任务时用了两个队列：输入队列 和 工作队列。
// 当向 MessageLoop 中内抛Task时，首先会将Task抛入MessageLoop的输入队列中，等到工作队列
// 处理完成之后，再将当前的输入队列放入工作队列中，继续执行。这样，就只有当将Task放入输入队列
// 时才需要加锁，而平时执行Task时是无锁的，这样就减少了对锁的占用时间。
// 2. 延时任务
// 为了实现延时任务，在MessageLoop中除了输入队列和工作队列，还有两个队列：延迟任务队列 和 需
// 在顶层执行的延迟任务队列。
// 在工作队列执行的时候，如果发现当前任务是延迟任务，则将任务放入此延迟队列，之后再处理，而如果
// 发现当前消息循环处于嵌套状态，而任务本身不允许嵌套，则放入需在顶层执行的延迟任务队列。
// 一旦MessagePump产生了执行延迟任务的回调，则将从这两个队列中拿出任务出来执行。

// 消息循环之 MessagePump:
// MessagePump 是用来从系统获取消息回调，触发 MessageLoop 执行 Task 的类，对于不同种类的
// MessageLoop都有一个相对应的 MessagePump，这是为了将不同线程的任务执行触发方式封装起来，
// 并且为 MessageLoop 提供跨平台的功能，chromium才将这个变化的部分封装成了MessagePump。所
// 以在 MessagePump 的实现中，大家就可以找到很多不同类型的 MessagePump，如：MessagePumpWin，
// MessagePumpLibEvent，这些就是不同平台上或者不同线程上的封装。
// Windows上的MessagePump有三种：MessagePumpDefault，MessagePumpForUI 和
// MessagePumpForIO，他们分别对应着MessageLoop，MessageLoopForUI和MessageLoopForIO。
// 下面我们从底层循环的实现，如何实现延时Task等等方面来看一下这些不同的MessagePump的实现方式：
// 1. MessagePumpDefault:
// MessagePumpDefault 是用于支持最基础的 MessageLoop 的消息泵，他中间其实是用一个for循环，
// 在中间死循环，每次循环都回调MessageLoop要求其处理新来的Task。不过这样CPU还不满了？当然
// Chromium 不会仅仅这么傻，在这个Pump中还申请了一个Event，在Task都执行完毕了之后，就会开始
// 等待这个Event，直到下个Task到来时SetEvent，或者通过等待超时到某个延迟Task可以被执行。
// 2. MessagePumpForUI
// MessagePumpForUI 是用于支持 MessageLoopForUI 的消息泵，他和默认消息泵的区别是他中间会
// 运行一个Windows的消息循环，用于分发窗口的消息，另外他还增加了一些和窗口相关的Observer等等。
// 各位应该也想到了一个问题：如果在某个任务中出现了模态对话框等等的Windows内部的消息循环，那么
// 新来的消息应该如何处理呢？其实在这个消息泵启动的时候，会创建一个消息窗口，一旦有新的任务到来，
// 都会像这个窗口Post一个消息，这样利用这个窗口，即便在Windows内部消息循环存在的情况下，也可以
// 正常触发执行Task的逻辑。
// 既然有了消息窗口，那么触发延时Task的就很简单了，只需要给这个窗口设置一个定时器就可以了。
// 各位应该也想到了一个问题：如果在某个任务中出现了模态对话框等等的Windows内部的消息循环，那么
// 新来的消息应该如何处理呢？
// 其实在这个消息泵启动的时候，会创建一个消息窗口，一旦有新的任务到来，都会像这个窗口Post一个消
// 息，这样利用这个窗口，即便在Windows内部消息循环存在的情况下，也可以正常触发执行Task的逻辑。
// 既然有了消息窗口，那么触发延时Task的就很简单了，只需要给这个窗口设置一个定时器就可以了。
// 3. MessagePumpForIO
// MessagePumpForIO 是用于支持 MessageLoopForIO 的消息泵，他和默认消息泵的区别在于，他底层
// 实际上是一个用完成端口组成的消息循环，这样不管是本地文件的读写，或者是网络收发，都可以看作是一
// 次IO事件。而一旦有任务或者有延时Task到来，这个消息泵就会向完成端口中发送一个自定义的IO事件，
// 从而触发MessageLoop处理Task。

namespace base {

class TimeTicks;

// 真正处理各种等待、唤醒、阻塞 和 处理事件 的则是在 MessagePump 中进行的。
// MessagePump适用于任务调度的，不愧于叫 “泵”.
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
    // ScheduleWork() 时，以先到者为准。允许在这些要求之外对 DoWork() 进行冗余/虚假调用。
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
    // 在消息泵开始等待工作之前调用。 这表明消息泵处于空闲状态（没有应用程序工作，
    // 理想情况下没有本地工作——如果可以的话）。
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
  // 在该函数中，消息泵负责处理本地消息以及定期向代理提供周期。
  // 消息泵应该注意将代理回调与本机消息处理混合在一起，这样任何一种类型的事件都不会饿
  // 死另一个循环。对委托函数的每次调用都被视为新“工作单元”的开始。
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
  // 实际的 消息循环 是在 MessagePump::Run 函数中进行的，如果有任务则执行任务，没有任务
  // 则在此 Run 函数中等待。
  // UI线程是比较特殊的，它是主线程，需要处理各种与用户的交互，所以它是不能阻塞，且与平台相关的。
  // 在各个平台下UI线程的MessagePumpForUI类的实现是不同的，使用平台提供的消息循环。比如
  // chromium for android 使用的是 android 提供的 handler 那一套.
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
  // 安排一个 DoWork() 回调以合理地尽快发生。如果已经安排了 DoWork 回调，则不执行任何操作。
  // 一旦进行此调用，DoWork 保证至少会被重复调用，直到它返回一个非立即的 NextWorkInfo。
  // 此调用可能代价高昂，调用者应尝试在从 DoWork() 返回非立即 NextWorkInfo 之前不要再次调
  // 用它。 线程安全（调用者在进行此调用时应不惜一切代价避免持有锁，因为已观察到某些平台的优先
  // 级提升功能会导致调用者被取消调度：https://crbug.com/890978）。
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
