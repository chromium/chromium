// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/message_loop/message_pump_mac.h"

// Foundation框架对于OC语言来说就是一个基础实现。是苹果官方的工程师们编写好的供OC
// 编程使用的一个框架，在这个框架的基础上，可以编写OC程序实现一定的功能.
// Foundation框架中的类都是以NS(NextStep)为前缀的，
// 学习：https://cloud.tencent.com/developer/article/1139804
#import <Foundation/Foundation.h>

#include <limits>
#include <memory>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/mac/call_with_eh_frame.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/message_loop/timer_slack.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if !defined(OS_IOS) // Mac平台
// Mac系统的应用程序使用的是AppKit框架，在AppKit里面可以看到Cocoa框架中关于用户界
// 面的大量资源，像UIKit一样，它包含了按钮、文本框等内容，尽管它关注的是macOS而不是
// iOS。
// 学习：https://www.jianshu.com/p/ef7b303d8596
// UIKit是用来开发iOS的应用的，AppKit是用来开发Mac应用的，在使用过程中他们很相似，
// 可是又有很多不同之处，通过对比分析它们的几个核心对象，可以避免混淆。
// UIKit和AppKit都有一个Application类，每个应用都只创建一个Application对象，分
// 别是UIAplication和NSApplication的实例。
#import <AppKit/AppKit.h>
#endif  // !defined(OS_IOS)

namespace base {

// 消息循环 “独占” 模式
const CFStringRef kMessageLoopExclusiveRunLoopMode =
    CFSTR("kMessageLoopExclusiveRunLoopMode");

namespace {

// 荒谬的松弛设置
MessagePumpCFRunLoopBase::LudicrousSlackSetting GetLudicrousSlackSetting() {
  return base::IsLudicrousTimerSlackEnabled()
             ? MessagePumpCFRunLoopBase::LudicrousSlackSetting::
                   kLudicrousSlackOn
             : MessagePumpCFRunLoopBase::LudicrousSlackSetting::
                   kLudicrousSlackOff;
}

// Mask that determines which modes to use.
enum {
  kCommonModeMask = 0x1, // 共模掩模
  kAllModesMask = 0xf    // 所有模式掩码
};

// Modes to use for MessagePumpNSApplication that are considered "safe".
// Currently just common and exclusive modes. Ideally, messages would be pumped
// in all modes, but that interacts badly with app modal dialogs (e.g. NSAlert).
// MessagePumpNSApplication 使用的被认为“安全”的模式。 目前只是普通模式和独占模式。
// 理想情况下，消息将以所有模式发送，但与应用程序模式对话框（例如 NSAlert）的交互很糟糕。
enum {
  kNSApplicationModalSafeModeMask = 0x3
};

// 无操作
void NoOp(void* info) {}

// 时间间隔最大值
constexpr CFTimeInterval kCFTimeIntervalMax =
    std::numeric_limits<CFTimeInterval>::max();

#if !defined(OS_IOS)
// Set to true if MessagePumpMac::Create() is called before NSApp is
// initialized.  Only accessed from the main thread.
// 如果在初始化 NSApp 之前调用 MessagePumpMac::Create()，则设置为 true。
// 只能从主线程访问。
bool g_not_using_cr_app = false;

// The MessagePump controlling [NSApp run].
// MessagePump 控制 [NSApp 运行]。
MessagePumpNSApplication* g_app_pump;
#endif  // !defined(OS_IOS)

}  // namespace

// A scoper for autorelease pools created from message pump run loops.
// Avoids dirtying up the ScopedNSAutoreleasePool interface for the rare
// case where an autorelease pool needs to be passed in.
// 从消息泵运行循环创建的自动释放池的范围。在需要传入自动释放池的极少数情况下，避免
// 弄脏 Scoped NSAutoreleasePool 接口。
class MessagePumpScopedAutoreleasePool {
 public:
  explicit MessagePumpScopedAutoreleasePool(MessagePumpCFRunLoopBase* pump) :
      pool_(pump->CreateAutoreleasePool()) {
  }

  MessagePumpScopedAutoreleasePool(const MessagePumpScopedAutoreleasePool&) = delete;
  MessagePumpScopedAutoreleasePool& operator=(
      const MessagePumpScopedAutoreleasePool&) = delete;

  ~MessagePumpScopedAutoreleasePool() {
    [pool_ drain]; // 释放
  }

 private:
 // 在引用计数环境中(与使用垃圾收集的环境相反)，NSAutoreleasePool对象包含已收到
 // 自动释放消息的对象，并且在耗尽时它会向每个对象发送释放消息。
  NSAutoreleasePool* pool_;
};

class MessagePumpCFRunLoopBase::ScopedModeEnabler {
 public:
  /**
   * CFRunLoop管理的类型通常有三种，这个函数中全部体现了：
   * 1. sources(CFRunLoopSource)     事件源
   * 2. timers(CFRunLoopTimer)       定时器
        是基于时间的触发器。和NSTimer类似，可以执行一些定时任务。
   * 3. observers(CFRunLoopObserver) 观察者
   */
  ScopedModeEnabler(MessagePumpCFRunLoopBase* owner, int mode_index)
      : owner_(owner), mode_index_(mode_index) {

    CFRunLoopRef loop = owner_->run_loop_;

    // 将 定时器(delayed_work_timer_) 事件添加到消息循环
    CFRunLoopAddTimer(loop, owner_->delayed_work_timer_, mode());

    // 将 work_source_源(事件)添加到消息循环
    CFRunLoopAddSource(loop, owner_->work_source_, mode());
    // 将 idle_work_source_源(事件)添加到消息循环
    CFRunLoopAddSource(loop, owner_->idle_work_source_, mode());
    // 忽略
    CFRunLoopAddSource(loop, owner_->nesting_deferred_work_source_, mode());

    // 添加观察者到消息循环
    CFRunLoopAddObserver(loop, owner_->pre_wait_observer_, mode());
    CFRunLoopAddObserver(loop, owner_->pre_source_observer_, mode());
    CFRunLoopAddObserver(loop, owner_->enter_exit_observer_, mode());
  }

  ScopedModeEnabler(const ScopedModeEnabler&) = delete;
  ScopedModeEnabler& operator=(const ScopedModeEnabler&) = delete;

  ~ScopedModeEnabler() {
    CFRunLoopRef loop = owner_->run_loop_;
    CFRunLoopRemoveObserver(loop, owner_->enter_exit_observer_, mode());
    CFRunLoopRemoveObserver(loop, owner_->pre_source_observer_, mode());
    CFRunLoopRemoveObserver(loop, owner_->pre_wait_observer_, mode());

    CFRunLoopRemoveSource(loop, owner_->nesting_deferred_work_source_, mode());
    CFRunLoopRemoveSource(loop, owner_->idle_work_source_, mode());
    CFRunLoopRemoveSource(loop, owner_->work_source_, mode());

    CFRunLoopRemoveTimer(loop, owner_->delayed_work_timer_, mode());
  }

  // This function knows about the AppKit RunLoop modes observed to potentially
  // run tasks posted to Chrome's main thread task runner. Some are internal to
  // AppKit but must be observed to keep Chrome's UI responsive. Others that may
  // be interesting, but are not watched:
  //  - com.apple.hitoolbox.windows.transitionmode
  //  - com.apple.hitoolbox.windows.flushmode
  const CFStringRef& mode() const {
    static const CFStringRef modes[] = {
        // The standard Core Foundation "common modes" constant. Must always be
        // first in this list to match the value of kCommonModeMask.
        // 标准的核心基础“通用模式”常量。 必须始终位于此列表的第一个以匹配
        // kCommonModeMask 的值。
        kCFRunLoopCommonModes,

        // Mode that only sees Chrome work sources.
        // 消息循环 “独占” 模式，仅查看 Chrome 工作源的模式。
        kMessageLoopExclusiveRunLoopMode,

        // Process work when NSMenus are fading out.
        CFSTR("com.apple.hitoolbox.windows.windowfadingmode"),

        // Process work when AppKit is highlighting an item on the main menubar.
        CFSTR("NSUnhighlightMenuRunLoopMode"),
    };
    static_assert(base::size(modes) == kNumModes, "mode size mismatch");
    static_assert((1 << kNumModes) - 1 == kAllModesMask,
                  "kAllModesMask not large enough");

    return modes[mode_index_];
  }

 private:
  MessagePumpCFRunLoopBase* const owner_;  // Weak. Owns this.
  const int mode_index_;
};

// Must be called on the run loop thread.
// 在消息循环线程中被调用
void MessagePumpCFRunLoopBase::Run(Delegate* delegate) {
  AutoReset<bool> auto_reset_keep_running(&keep_running_, true);
  // nesting_level_ will be incremented in EnterExitRunLoop, so set
  // run_nesting_level_ accordingly.
  int last_run_nesting_level = run_nesting_level_;
  run_nesting_level_ = nesting_level_ + 1;

  Delegate* last_delegate = delegate_;
  SetDelegate(delegate);

  ScheduleWork();
  DoRun(delegate); // 这里调用的是子类(iOS/Android)

  // Restore the previous state of the object.
  SetDelegate(last_delegate);
  run_nesting_level_ = last_run_nesting_level;
}

void MessagePumpCFRunLoopBase::Quit() {
  if (DoQuit())
    OnDidQuit();
}

void MessagePumpCFRunLoopBase::OnDidQuit() {
  keep_running_ = false;
}

// May be called on any thread.
void MessagePumpCFRunLoopBase::ScheduleWork() {
  // 向 work_source_(CFRunLoopSourceRef) 事件发出信号，将其标记为准备触发
  CFRunLoopSourceSignal(work_source_);
  // 唤醒一个等待的 (run_loop_)CFRunLoop 对象
  CFRunLoopWakeUp(run_loop_);
}

// Must be called on the run loop thread.
void MessagePumpCFRunLoopBase::ScheduleDelayedWork(
    const TimeTicks& delayed_work_time) {
  ScheduleDelayedWorkImpl(delayed_work_time - TimeTicks::Now());
}

MessagePumpCFRunLoopBase::LudicrousSlackSetting
MessagePumpCFRunLoopBase::GetLudicrousSlackState() const {
  if (ludicrous_slack_setting_ == LudicrousSlackSetting::kLudicrousSlackOn &&
      IsLudicrousTimerSlackSuspended()) {
    return LudicrousSlackSetting::kLudicrousSlackSuspended;
  }

  return ludicrous_slack_setting_;
}

void MessagePumpCFRunLoopBase::ScheduleDelayedWorkImpl(TimeDelta delta) {
  // The tolerance needs to be set before the fire date or it may be ignored.

  // Pickup the ludicrous slack setting as late as possible to work around
  // initialization issues in base. Note that the main thread won't sleep until
  // field trial initialization is complete.
  // 尽可能晚地使用可笑的 slack 设置来解决 base 中的初始化问题。
  // 请注意，在现场试验初始化完成之前，主线程不会休眠。
  if (ludicrous_slack_setting_ ==
      LudicrousSlackSetting::kLudicrousSlackUninitialized) {
    ludicrous_slack_setting_ = GetLudicrousSlackSetting();
  } else {
    // Validate that the setting doesn't change after we cache it.
    DCHECK_EQ(ludicrous_slack_setting_, GetLudicrousSlackSetting());
  }
  DCHECK_NE(ludicrous_slack_setting_,
            LudicrousSlackSetting::kLudicrousSlackUninitialized);

  if (GetLudicrousSlackState() == LudicrousSlackSetting::kLudicrousSlackOn) {
    // Specify ludicrous slack when the experiment is enabled and not suspended.
    // 在实验启用且未暂停时指定可笑的 slack。
    // 设置运行时间误差范围
    CFRunLoopTimerSetTolerance(delayed_work_timer_,
                               GetLudicrousTimerSlack().InSecondsF());
  } else if (timer_slack_ == TIMER_SLACK_MAXIMUM) {
    // 设置运行时间误差范围
    CFRunLoopTimerSetTolerance(delayed_work_timer_, delta.InSecondsF() * 0.5);
  } else {
    // 设置运行时间误差范围
    CFRunLoopTimerSetTolerance(delayed_work_timer_, 0);
  }
  CFRunLoopTimerSetNextFireDate(
      delayed_work_timer_,
      CFAbsoluteTimeGetCurrent() + delta.InSecondsF());
}

void MessagePumpCFRunLoopBase::SetTimerSlack(TimerSlack timer_slack) {
  timer_slack_ = timer_slack;
}

#if defined(OS_IOS)
void MessagePumpCFRunLoopBase::Attach(Delegate* delegate) {}

void MessagePumpCFRunLoopBase::Detach() {}
#endif  // OS_IOS

// Must be called on the run loop thread.
// 在run loop线程中被调用
MessagePumpCFRunLoopBase::MessagePumpCFRunLoopBase(int initial_mode_mask)
    : delegate_(NULL),
      timer_slack_(base::TIMER_SLACK_NONE),
      nesting_level_(0),
      run_nesting_level_(0),
      deepest_nesting_level_(0),
      keep_running_(true),
      delegateless_work_(false),
      delegateless_idle_work_(false) {

  // 返回当前线程的 CFRunLoop 对象
  run_loop_ = CFRunLoopGetCurrent();
  CFRetain(run_loop_); // 持有run_loop_实例，不让释放

  // Set a repeating timer with a preposterous firing time and interval.  The
  // timer will effectively never fire as-is.  The firing time will be adjusted
  // as needed when ScheduleDelayedWork is called.
  // 设置一个具有荒谬的触发时间和间隔的重复计时器。 计时器实际上永远不会按原样触发。
  // 调用 ScheduleDelayedWork 时会根据需要调整触发时间。
  // 包含程序定义的数据和回调的结构，您可以使用它们配置 CFRunLoopTimer 的行为。
  CFRunLoopTimerContext timer_context = CFRunLoopTimerContext();
  timer_context.info = this; // 将当前对象作为参数传入
  // 创建一个新的 CFRunLoopTimer 对象
  delayed_work_timer_ = CFRunLoopTimerCreate(NULL, // allocator 用于分配内存，通常使用kCFAllocatorDefault即可
                                             kCFTimeIntervalMax, // fire time 第一次触发调用的时间
                                             kCFTimeIntervalMax, // interval 回调间隔
                                             0, // flags 苹果备用参数，传0即可
                                             0, // priority RunLoop执行事件的优先级，对于Timer是无用的，传0即可
                                             RunDelayedWorkTimer, // 定时器事件的回调函数
                                             &timer_context); // 用于与callback联系的上下文context

  // 包含程序定义的数据和回调的结构，您可以使用它们配置版本 0 CFRunLoopSource 的行为
  CFRunLoopSourceContext source_context = CFRunLoopSourceContext();
  source_context.info = this;
  source_context.perform = RunWorkSource; // 执行工作回调函数
  // 创建工作源(事件)
  work_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                       1,     // priority
                                       &source_context);

  source_context.perform = RunIdleWorkSource; // idel工作的回调函数
  // 创建idle工作源(事件)
  idle_work_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                            2,     // priority
                                            &source_context);

  source_context.perform = RunNestingDeferredWorkSource; // 忽略，嵌套任务
  nesting_deferred_work_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                                        0,     // priority
                                                        &source_context);

  // 消息循环-观察者上下文，观察消息循环状态变化
  CFRunLoopObserverContext observer_context = CFRunLoopObserverContext();
  observer_context.info = this; // 把自己传递过去
  // 创建消息队列休眠等待之前观察者
  pre_wait_observer_ = CFRunLoopObserverCreate(NULL,  // allocator
                                               // 即将进入休眠
                                               kCFRunLoopBeforeWaiting,
                                               true,  // repeat
                                               0,     // priority
                                               // 回调函数
                                               PreWaitObserver,
                                               &observer_context);
  // 创建消息事件发生之前观察者
  pre_source_observer_ = CFRunLoopObserverCreate(NULL,  // allocator
                                                 // 即将处理 Source(事件)
                                                 kCFRunLoopBeforeSources,
                                                 true,  // repeat
                                                 0,     // priority
                                                 // 回调函数
                                                 PreSourceObserver,
                                                 &observer_context);
  // 进入退出消息循环观察者
  enter_exit_observer_ = CFRunLoopObserverCreate(NULL,  // allocator
                                                 // 即将进入Loop | 即将退出Loop
                                                 kCFRunLoopEntry | kCFRunLoopExit,
                                                 true,  // repeat
                                                 0,     // priority
                                                 // 回调函数
                                                 EnterExitObserver, // callback
                                                 &observer_context);
  SetModeMask(initial_mode_mask);
}

// Ideally called on the run loop thread.  If other run loops were running
// lower on the run loop thread's stack when this object was created, the
// same number of run loops must be running when this object is destroyed.
MessagePumpCFRunLoopBase::~MessagePumpCFRunLoopBase() {
  SetModeMask(0);
  CFRelease(enter_exit_observer_);
  CFRelease(pre_source_observer_);
  CFRelease(pre_wait_observer_);
  CFRelease(nesting_deferred_work_source_);
  CFRelease(idle_work_source_);
  CFRelease(work_source_);
  CFRelease(delayed_work_timer_);
  CFRelease(run_loop_);
}

void MessagePumpCFRunLoopBase::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;

  if (delegate) {
    // If any work showed up but could not be dispatched for want of a
    // delegate, set it up for dispatch again now that a delegate is
    // available.
    // 如果有任何工作出现但由于缺少委托而无法分派，则在委托可用后再次将其设置为分派。
    if (delegateless_work_) {
      // 向 work_source_(CFRunLoopSourceRef) 对象发出信号，将其标记为准备触发
      CFRunLoopSourceSignal(work_source_);
      delegateless_work_ = false;
    }
    if (delegateless_idle_work_) {
      // 向 idle_work_source_(CFRunLoopSourceRef) 对象发出信号，将其标记为准备触发
      CFRunLoopSourceSignal(idle_work_source_);
      delegateless_idle_work_ = false;
    }
  }
}

// Base version returns a standard NSAutoreleasePool.
AutoreleasePoolType* MessagePumpCFRunLoopBase::CreateAutoreleasePool() {
  return [[NSAutoreleasePool alloc] init];
}

void MessagePumpCFRunLoopBase::SetModeMask(int mode_mask) {
  for (size_t i = 0; i < kNumModes; ++i) {
    bool enable = mode_mask & (0x1 << i);
    if (enable == !enabled_modes_[i]) {
      enabled_modes_[i] = enable ?
          std::make_unique<ScopedModeEnabler>(this, i) : nullptr;
    }
  }
}

int MessagePumpCFRunLoopBase::GetModeMask() const {
  int mask = 0;
  for (size_t i = 0; i < kNumModes; ++i)
    mask |= enabled_modes_[i] ? (0x1 << i) : 0;
  return mask;
}

// Called from the run loop. CFRunLoopTimerCreate的定时器回调函数
// static
void MessagePumpCFRunLoopBase::RunDelayedWorkTimer(CFRunLoopTimerRef timer,
                                                   void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);
  // The timer fired, assume we have work and let RunWork() figure out what to
  // do and what to schedule after.
  base::mac::CallWithEHFrame(^{
    self->RunWork();
  });
}

// Called from the run loop. 在消息循环线程中被调用
// static
void MessagePumpCFRunLoopBase::RunWorkSource(void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);
  base::mac::CallWithEHFrame(^{
    self->RunWork();
  });
}

// Called by MessagePumpCFRunLoopBase::RunWorkSource and RunDelayedWorkTimer.
bool MessagePumpCFRunLoopBase::RunWork() {
  if (!delegate_) {
    // This point can be reached with a nullptr |delegate_| if Run is not on the
    // stack but foreign code is spinning the CFRunLoop.  Arrange to come back
    // here when a delegate is available.
    delegateless_work_ = true;
    return false;
  }
  if (!keep_running())
    return false;

  // The NSApplication-based run loop only drains the autorelease pool at each
  // UI event (NSEvent).  The autorelease pool is not drained for each
  // CFRunLoopSource target that's run.  Use a local pool for any autoreleased
  // objects if the app is not currently handling a UI event to ensure they're
  // released promptly even in the absence of UI events.
  MessagePumpScopedAutoreleasePool autorelease_pool(this);

  // 执行代理的消息队列中的任务
  Delegate::NextWorkInfo next_work_info = delegate_->DoWork();
  if (next_work_info.is_immediate()) {
    // 向 work_source_(CFRunLoopSourceRef) 对象发出信号，将其标记为准备触发
    CFRunLoopSourceSignal(work_source_);
    return true;
  }

  // 定时器任务
  if (!next_work_info.delayed_run_time.is_max())
    ScheduleDelayedWorkImpl(next_work_info.remaining_delay());
  return false;
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::RunIdleWorkSource(void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);
  base::mac::CallWithEHFrame(^{
    self->RunIdleWork();
  });
}

// Called by MessagePumpCFRunLoopBase::RunIdleWorkSource.
void MessagePumpCFRunLoopBase::RunIdleWork() {
  if (!delegate_) {
    // This point can be reached with a nullptr delegate_ if Run is not on the
    // stack but foreign code is spinning the CFRunLoop.  Arrange to come back
    // here when a delegate is available.
    delegateless_idle_work_ = true;
    return;
  }
  if (!keep_running())
    return;
  // The NSApplication-based run loop only drains the autorelease pool at each
  // UI event (NSEvent).  The autorelease pool is not drained for each
  // CFRunLoopSource target that's run.  Use a local pool for any autoreleased
  // objects if the app is not currently handling a UI event to ensure they're
  // released promptly even in the absence of UI events.
  MessagePumpScopedAutoreleasePool autorelease_pool(this);
  // Call DoIdleWork once, and if something was done, arrange to come back here
  // again as long as the loop is still running.
  bool did_work = delegate_->DoIdleWork();
  if (did_work) {
    // 向 idle_work_source_(CFRunLoopSourceRef) 对象发出信号，将其标记为准备触发
    CFRunLoopSourceSignal(idle_work_source_);
  }
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::RunNestingDeferredWorkSource(void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);
  base::mac::CallWithEHFrame(^{
    self->RunNestingDeferredWork();
  });
}

// Called by MessagePumpCFRunLoopBase::RunNestingDeferredWorkSource.
void MessagePumpCFRunLoopBase::RunNestingDeferredWork() {
  if (!delegate_) {
    // This point can be reached with a nullptr |delegate_| if Run is not on the
    // stack but foreign code is spinning the CFRunLoop.  There's no sense in
    // attempting to do any work or signalling the work sources because
    // without a delegate, work is not possible.
    return;
  }

  if (RunWork()) {
    // Work was done.  Arrange for the loop to try non-nestable idle work on
    // a subsequent pass.
    CFRunLoopSourceSignal(idle_work_source_);
  } else {
    RunIdleWork();
  }
}

void MessagePumpCFRunLoopBase::BeforeWait() {
  if (!delegate_) {
    // This point can be reached with a nullptr |delegate_| if Run is not on the
    // stack but foreign code is spinning the CFRunLoop.
    return;
  }
  delegate_->BeforeWait();
}

// Called before the run loop goes to sleep or exits, or processes sources.
void MessagePumpCFRunLoopBase::MaybeScheduleNestingDeferredWork() {
  // deepest_nesting_level_ is set as run loops are entered.  If the deepest
  // level encountered is deeper than the current level, a nested loop
  // (relative to the current level) ran since the last time nesting-deferred
  // work was scheduled.  When that situation is encountered, schedule
  // nesting-deferred work in case any work was deferred because nested work
  // was disallowed.
  if (deepest_nesting_level_ > nesting_level_) {
    deepest_nesting_level_ = nesting_level_;
    // 把 nesting_deferred_work_source_源标记位待处理，等到手动调用 CFRunLoopWakeUp即可
    CFRunLoopSourceSignal(nesting_deferred_work_source_);
  }
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::PreWaitObserver(CFRunLoopObserverRef observer,
                                               CFRunLoopActivity activity,
                                               void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);
  base::mac::CallWithEHFrame(^{
    // Attempt to do some idle work before going to sleep.
    self->RunIdleWork();

    // The run loop is about to go to sleep.  If any of the work done since it
    // started or woke up resulted in a nested run loop running,
    // nesting-deferred work may have accumulated.  Schedule it for processing
    // if appropriate.
    self->MaybeScheduleNestingDeferredWork();

    // Notify the delegate that the loop is about to sleep.
    self->BeforeWait();
  });
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::PreSourceObserver(CFRunLoopObserverRef observer,
                                                 CFRunLoopActivity activity,
                                                 void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);

  // The run loop has reached the top of the loop and is about to begin
  // processing sources.  If the last iteration of the loop at this nesting
  // level did not sleep or exit, nesting-deferred work may have accumulated
  // if a nested loop ran.  Schedule nesting-deferred work for processing if
  // appropriate.
  base::mac::CallWithEHFrame(^{
    self->MaybeScheduleNestingDeferredWork();
  });
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::EnterExitObserver(CFRunLoopObserverRef observer,
                                                 CFRunLoopActivity activity,
                                                 void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);

  switch (activity) {
    case kCFRunLoopEntry: // 即将进入消息循环事件
      ++self->nesting_level_;
      if (self->nesting_level_ > self->deepest_nesting_level_) {
        self->deepest_nesting_level_ = self->nesting_level_;
      }
      break;

    case kCFRunLoopExit: // 即将退出消息循环事件
      // Not all run loops go to sleep.  If a run loop is stopped before it
      // goes to sleep due to a CFRunLoopStop call, or if the timeout passed
      // to CFRunLoopRunInMode expires, the run loop may proceed directly from
      // handling sources to exiting without any sleep.  This most commonly
      // occurs when CFRunLoopRunInMode is passed a timeout of 0, causing it
      // to make a single pass through the loop and exit without sleep.  Some
      // native loops use CFRunLoop in this way.  Because PreWaitObserver will
      // not be called in these case, MaybeScheduleNestingDeferredWork needs
      // to be called here, as the run loop exits.
      //
      // MaybeScheduleNestingDeferredWork consults self->nesting_level_
      // to determine whether to schedule nesting-deferred work.  It expects
      // the nesting level to be set to the depth of the loop that is going
      // to sleep or exiting.  It must be called before decrementing the
      // value so that the value still corresponds to the level of the exiting
      // loop.
      base::mac::CallWithEHFrame(^{
        self->MaybeScheduleNestingDeferredWork();
      });
      --self->nesting_level_;
      break;

    default:
      break;
  }

  base::mac::CallWithEHFrame(^{
    self->EnterExitRunLoop(activity);
  });
}

// Called by MessagePumpCFRunLoopBase::EnterExitRunLoop.  The default
// implementation is a no-op.
void MessagePumpCFRunLoopBase::EnterExitRunLoop(CFRunLoopActivity activity) {}

MessagePumpCFRunLoop::MessagePumpCFRunLoop()
    : MessagePumpCFRunLoopBase(kCommonModeMask), quit_pending_(false) {}

MessagePumpCFRunLoop::~MessagePumpCFRunLoop() {}

// Called by MessagePumpCFRunLoopBase::DoRun.  If other CFRunLoopRun loops were
// running lower on the run loop thread's stack when this object was created,
// the same number of CFRunLoopRun loops must be running for the outermost call
// to Run.  Run/DoRun are reentrant after that point.
void MessagePumpCFRunLoop::DoRun(Delegate* delegate) {
  // This is completely identical to calling CFRunLoopRun(), except autorelease
  // pool management is introduced.
  int result;
  do {
    MessagePumpScopedAutoreleasePool autorelease_pool(this);
    result = CFRunLoopRunInMode(kCFRunLoopDefaultMode,
                                kCFTimeIntervalMax,
                                false);
  } while (result != kCFRunLoopRunStopped && result != kCFRunLoopRunFinished);
}

// Must be called on the run loop thread.
// 必须是在运行时线程中被调用
bool MessagePumpCFRunLoop::DoQuit() {
  // Stop the innermost run loop managed by this MessagePumpCFRunLoop object.
  if (nesting_level() == run_nesting_level()) {
    // This object is running the innermost loop, just stop it.
    // 强制CFRunLoop对象停止运行
    CFRunLoopStop(run_loop());
    return true;
  } else {
    // There's another loop running inside the loop managed by this object.
    // In other words, someone else called CFRunLoopRunInMode on the same
    // thread, deeper on the stack than the deepest Run call.  Don't preempt
    // other run loops, just mark this object to quit the innermost Run as
    // soon as the other inner loops not managed by Run are done.
    quit_pending_ = true;
    return false;
  }
}

// Called by MessagePumpCFRunLoopBase::EnterExitObserver.
void MessagePumpCFRunLoop::EnterExitRunLoop(CFRunLoopActivity activity) {
  if (activity == kCFRunLoopExit &&
      nesting_level() == run_nesting_level() &&
      quit_pending_) {
    // Quit was called while loops other than those managed by this object
    // were running further inside a run loop managed by this object.  Now
    // that all unmanaged inner run loops are gone, stop the loop running
    // just inside Run.
    // 强制CFRunLoop对象停止运行
    CFRunLoopStop(run_loop());
    quit_pending_ = false;
    OnDidQuit();
  }
}

MessagePumpNSRunLoop::MessagePumpNSRunLoop()
    : MessagePumpCFRunLoopBase(kCommonModeMask) {
  // 创建源（事件）上下文
  CFRunLoopSourceContext source_context = CFRunLoopSourceContext();
  source_context.perform = NoOp; // 回调函数设置为空函数
  // 创建退出源(事件)
  quit_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                       0,     // priority
                                       &source_context);
  // 添加退出source(退出事件)到消息循环中，退出事件通知时，这里就可以收到
  CFRunLoopAddSource(run_loop(), quit_source_, kCFRunLoopCommonModes);
}

MessagePumpNSRunLoop::~MessagePumpNSRunLoop() {
  CFRunLoopRemoveSource(run_loop(), quit_source_, kCFRunLoopCommonModes);
  CFRelease(quit_source_);
}

void MessagePumpNSRunLoop::DoRun(Delegate* delegate) {
  while (keep_running()) {
    // NSRunLoop manages autorelease pools itself.
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate distantFuture]];
  }
}

bool MessagePumpNSRunLoop::DoQuit() {
  // 唤醒休眠的消息循环，退出消息循环
  CFRunLoopSourceSignal(quit_source_);
  CFRunLoopWakeUp(run_loop());
  return true;
}

#if defined(OS_IOS)
MessagePumpUIApplication::MessagePumpUIApplication()
    : MessagePumpCFRunLoopBase(kCommonModeMask), run_loop_(NULL) {}

MessagePumpUIApplication::~MessagePumpUIApplication() {}

void MessagePumpUIApplication::DoRun(Delegate* delegate) {
  NOTREACHED();
}

bool MessagePumpUIApplication::DoQuit() {
  NOTREACHED();
  return false;
}

void MessagePumpUIApplication::Attach(Delegate* delegate) {
  DCHECK(!run_loop_);
  run_loop_ = new RunLoop();
  CHECK(run_loop_->BeforeRun());
  SetDelegate(delegate);
}

void MessagePumpUIApplication::Detach() {
  DCHECK(run_loop_);
  run_loop_->AfterRun();
  SetDelegate(nullptr);
  run_loop_ = nullptr;
}

#else

ScopedPumpMessagesInPrivateModes::ScopedPumpMessagesInPrivateModes() {
  DCHECK(g_app_pump);
  DCHECK_EQ(kNSApplicationModalSafeModeMask, g_app_pump->GetModeMask());
  // Pumping events in private runloop modes is known to interact badly with
  // app modal windows like NSAlert.
  if ([NSApp modalWindow])
    return;
  g_app_pump->SetModeMask(kAllModesMask);
}

ScopedPumpMessagesInPrivateModes::~ScopedPumpMessagesInPrivateModes() {
  DCHECK(g_app_pump);
  g_app_pump->SetModeMask(kNSApplicationModalSafeModeMask);
}

int ScopedPumpMessagesInPrivateModes::GetModeMaskForTest() {
  return g_app_pump ? g_app_pump->GetModeMask() : -1;
}

MessagePumpNSApplication::MessagePumpNSApplication()
    : MessagePumpCFRunLoopBase(kNSApplicationModalSafeModeMask),
      running_own_loop_(false),
      quit_pending_(false) {
  DCHECK_EQ(nullptr, g_app_pump);
  g_app_pump = this;
}

MessagePumpNSApplication::~MessagePumpNSApplication() {
  DCHECK_EQ(this, g_app_pump);
  g_app_pump = nullptr;
}

void MessagePumpNSApplication::DoRun(Delegate* delegate) {
  bool last_running_own_loop_ = running_own_loop_;

  // NSApp must be initialized by calling:
  // [{some class which implements CrAppProtocol} sharedApplication]
  // Most likely candidates are CrApplication or BrowserCrApplication.
  // These can be initialized from C++ code by calling
  // RegisterCrApp() or RegisterBrowserCrApp().
  CHECK(NSApp);

  if (![NSApp isRunning]) {
    running_own_loop_ = false;
    // NSApplication manages autorelease pools itself when run this way.
    [NSApp run]; // 开始执行mac/ios的消息循环
  } else {
    running_own_loop_ = true;
    NSDate* distant_future = [NSDate distantFuture];
    while (keep_running()) {
      MessagePumpScopedAutoreleasePool autorelease_pool(this);
      NSEvent* event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                          untilDate:distant_future
                                             inMode:NSDefaultRunLoopMode
                                            dequeue:YES];
      if (event) {
        [NSApp sendEvent:event];
      }
    }
  }

  running_own_loop_ = last_running_own_loop_;
}

bool MessagePumpNSApplication::DoQuit() {
  // If the app is displaying a modal window in a native run loop, we can only
  // quit our run loop after the window is closed. Otherwise the [NSApplication
  // stop] below will apply to the modal window run loop instead. To work around
  // this, the quit is applied when we re-enter our own run loop after the
  // window is gone (see MessagePumpNSApplication::EnterExitRunLoop).
  if (nesting_level() > run_nesting_level() &&
      [[NSApplication sharedApplication] modalWindow] != nil) {
    quit_pending_ = true;
    return false;
  }

  if (!running_own_loop_) {
    [[NSApplication sharedApplication] stop:nil];
  }

  // Send a fake event to wake the loop up.
  [NSApp postEvent:[NSEvent otherEventWithType:NSApplicationDefined
                                      location:NSZeroPoint
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:NULL
                                       subtype:0
                                         data1:0
                                         data2:0]
           atStart:NO];
  return true;
}

void MessagePumpNSApplication::EnterExitRunLoop(CFRunLoopActivity activity) {
  // If we previously tried quitting while a modal window was active, check if
  // the window is gone now and we're no longer nested in a system run loop.
  if (activity == kCFRunLoopEntry && quit_pending_ &&
      nesting_level() <= run_nesting_level() &&
      [[NSApplication sharedApplication] modalWindow] == nil) {
    quit_pending_ = false;
    if (DoQuit())
      OnDidQuit();
  }
}

MessagePumpCrApplication::MessagePumpCrApplication() {}

MessagePumpCrApplication::~MessagePumpCrApplication() {}

// Prevents an autorelease pool from being created if the app is in the midst of
// handling a UI event because various parts of AppKit depend on objects that
// are created while handling a UI event to be autoreleased in the event loop.
// An example of this is NSWindowController. When a window with a window
// controller is closed it goes through a stack like this:
// (Several stack frames elided for clarity)
//
// #0 [NSWindowController autorelease]
// #1 DoAClose
// #2 MessagePumpCFRunLoopBase::DoWork()
// #3 [NSRunLoop run]
// #4 [NSButton performClick:]
// #5 [NSWindow sendEvent:]
// #6 [NSApp sendEvent:]
// #7 [NSApp run]
//
// -performClick: spins a nested run loop. If the pool created in DoWork was a
// standard NSAutoreleasePool, it would release the objects that were
// autoreleased into it once DoWork released it. This would cause the window
// controller, which autoreleased itself in frame #0, to release itself, and
// possibly free itself. Unfortunately this window controller controls the
// window in frame #5. When the stack is unwound to frame #5, the window would
// no longer exists and crashes may occur. Apple gets around this by never
// releasing the pool it creates in frame #4, and letting frame #7 clean it up
// when it cleans up the pool that wraps frame #7. When an autorelease pool is
// released it releases all other pools that were created after it on the
// autorelease pool stack.
//
// CrApplication is responsible for setting handlingSendEvent to true just
// before it sends the event through the event handling mechanism, and
// returning it to its previous value once the event has been sent.
AutoreleasePoolType* MessagePumpCrApplication::CreateAutoreleasePool() {
  if (MessagePumpMac::IsHandlingSendEvent())
    return nil;
  return MessagePumpNSApplication::CreateAutoreleasePool();
}

// static
bool MessagePumpMac::UsingCrApp() {
  DCHECK([NSThread isMainThread]);

  // If NSApp is still not initialized, then the subclass used cannot
  // be determined.
  DCHECK(NSApp);

  // The pump was created using MessagePumpNSApplication.
  if (g_not_using_cr_app)
    return false;

  return [NSApp conformsToProtocol:@protocol(CrAppProtocol)];
}

// static
bool MessagePumpMac::IsHandlingSendEvent() {
  DCHECK([NSApp conformsToProtocol:@protocol(CrAppProtocol)]);
  NSObject<CrAppProtocol>* app = static_cast<NSObject<CrAppProtocol>*>(NSApp);
  return [app isHandlingSendEvent];
}
#endif  // !defined(OS_IOS)

// 创建用于处理iOS和Mac平台的UI事件的消息泵
// static
std::unique_ptr<MessagePump> MessagePumpMac::Create() {
  if ([NSThread isMainThread]) { // 主线程
#if defined(OS_IOS) // iOS平台
    return std::make_unique<MessagePumpUIApplication>();
#else // Mac平台
    if ([NSApp conformsToProtocol:@protocol(CrAppProtocol)])
      return std::make_unique<MessagePumpCrApplication>();

    // The main-thread MessagePump implementations REQUIRE an NSApp.
    // Executables which have specific requirements for their
    // NSApplication subclass should initialize appropriately before
    // creating an event loop.
    // 主线程 MessagePump 实现需要一个 NSApp。对其 NSApplication 子类有特定
    // 要求的可执行文件应在创建事件循环之前进行适当的初始化。
    [NSApplication sharedApplication];
    g_not_using_cr_app = true;
    return std::make_unique<MessagePumpNSApplication>();
#endif
  }

  return std::make_unique<MessagePumpNSRunLoop>();
}

}  // namespace base
