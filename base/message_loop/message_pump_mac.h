// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The basis for all native run loops on the Mac is the CFRunLoop.  It can be
// used directly, it can be used as the driving force behind the similar
// Foundation NSRunLoop, and it can be used to implement higher-level event
// loops such as the NSApplication event loop.
// Mac 上所有本机运行循环的基础是 CFRunLoop。可以直接使用，可以作为类似
// Foundation NSRunLoop背后的驱动，也可以用来实现NSApplication事件循环等更高级
// 的事件循环。
//
// This file introduces a basic CFRunLoop-based implementation of the
// MessagePump interface called CFRunLoopBase.  CFRunLoopBase contains all
// of the machinery necessary to dispatch events to a delegate, but does not
// implement the specific run loop.  Concrete subclasses must provide their
// own DoRun and DoQuit implementations.
// 该文件介绍了一个名为 CFRunLoopBase 的 MessagePump 接口的基于 CFRunLoop
// 的基本实现。CFRunLoopBase 包含将事件分派给委托所需的所有机制，但不实现特
// 定的运行循环。具体的子类必须提供它们自己的 DoRun 和 DoQuit 实现。
//
// A concrete subclass that just runs a CFRunLoop loop is provided in
// MessagePumpCFRunLoop.  For an NSRunLoop, the similar MessagePumpNSRunLoop
// is provided.
// MessagePumpCFRunLoop 中提供了一个只运行 CFRunLoop 循环的具体子类。
// 对于 NSRunLoop，提供了类似的 MessagePumpNSRunLoop。
//
// For the application's event loop, an implementation based on AppKit's
// NSApplication event system is provided in MessagePumpNSApplication.
// 对于应用程序的事件循环，MessagePumpNSApplication 中提供了基于 AppKit 的
// NSApplication 事件系统的实现。
//
// Typically, MessagePumpNSApplication only makes sense on a Cocoa
// application's main thread.  If a CFRunLoop-based message pump is needed on
// any other thread, one of the other concrete subclasses is preferable.
// MessagePumpMac::Create is defined, which returns a new NSApplication-based
// or NSRunLoop-based MessagePump subclass depending on which thread it is
// called on.
// 通常，MessagePumpNSApplication 仅在 Cocoa 应用程序的主线程上才有意义。
// 如果在任何其他线程上需要基于 CFRunLoop 的消息泵，则最好使用其他具体子类之一。
// MessagePumpMac::Create 被定义，它返回一个新的基于 NSApplication 或基于
// NSRunLoop 的 MessagePump 子类，具体取决于调用它的线程。

/**
 * @brief
 * 基础知识：
 * CFRunLoop
 * 学习: https://juejin.cn/post/6844904100627251208
 *      https://juejin.cn/post/6844904102665650189
 * CFRunLoop的概念:
 * 简单来说，CFRunLoop对象负责监控事件输入源以及对其进行分发管理。
 * CFRunLoop管理的类型通常分为sources(CFRunLoopSource)、timers(CFRunLoopTimer)
 * 和observers(CFRunLoopObserver)三种类型。
 * CFRunLoopSource:
 * CFRunLoopSourceRef是产生事件的地方。Source包括Source0和Source1两个版本。
 * (1) Source0：主要由应用程序管理，它并不能主动触发事件。使用时，你需要先调用
 * CFRunLoopSourceSignal(source)，将这个Source标记为待处理，然后手动调用
 * CFRunLoopWakeUp(runloop)来唤醒RunLoop，让其处理这个事件。通常我们使用的也是
 * Source0事件。
 * (2) Source1：主要由于RunLoop和kernel进行管理。包含了一个mach_port和一个回调
 * （函数指针），被用于通过内核和其他线程相互发送消息。这种Source能主动唤醒RunLoop
 * 的线程。
 *
 * NSApplication 是一个管理应用的时间循环和所用资源的对象，每一个应用都用了
 * 一个NSApplication类型的对象去控制事件循环，监听和更新应用的各个窗口（windows）
 * 和菜单，将事件分配到恰当的对象（即他自己或者他的一个window），生成自动释放池，接收
 * 应用级别（app-level）事件的通知。NSApplication对象有一个delegate(一个你分配的
 * 对象)，应用程序启动、终止、**、隐藏、用户要打开文件等生命周期事件会在这个delegate
 * 里收到通知。通过设置委托,实施委托方法,您定制你的应用程序的行为,而无需创建
 * NSApplication的子类。在你的应用程序的main()函数中,你可以通过调用shared类方法创
 * 建NSApplication实例。创建应用程序对象后,main()函数会加载应用程序的主nib文件，
 * 然后调用循环run()消息，开始应用程序对象的事件循环（event loop）。
 *
 * 在iOS中，NSRunLoop 和 CFRunLoopRef 就是实现“消息循环机制”的对象，其实
 * NSRunLoop 本质是由 CFRunLoopRef 封装的，提供了面向对象的API，而
 * CFRunLoopRef 是一些面向过程的C函数API。两者最主要的区别在于：NSRunLoop是非线
 * 程安全的，意味着你不能用非当前线程去调用当前线程的NSRunLoop，否则会出现意想不到
 * 的错误(You should never try to call the methods of an NSRunLoop object
 * running in a different thread)。而 CFRunLoopRef 是线程安全的。
 */

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_MAC_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_MAC_H_

#include "base/message_loop/message_pump.h"

#include <CoreFoundation/CoreFoundation.h>
#include <memory>

#include "base/macros.h"
#include "base/message_loop/timer_slack.h"
#include "build/build_config.h"

#if defined(__OBJC__)
#if defined(OS_IOS)
#import <Foundation/Foundation.h>
#else
#import <AppKit/AppKit.h>

// Clients must subclass NSApplication and implement this protocol if they use
// MessagePumpMac.
@protocol CrAppProtocol
// Must return true if -[NSApplication sendEvent:] is currently on the stack.
// See the comment for |CreateAutoreleasePool()| in the cc file for why this is
// necessary.
- (BOOL)isHandlingSendEvent;
@end
#endif  // !defined(OS_IOS)
#endif  // defined(__OBJC__)

namespace base {

class RunLoop;
class TimeTicks;

// AutoreleasePoolType is a proxy type for autorelease pools. Its definition
// depends on the translation unit (TU) in which this header appears. In pure
// C++ TUs, it is defined as a forward C++ class declaration (that is never
// defined), because autorelease pools are an Objective-C concept. In Automatic
// Reference Counting (ARC) Objective-C TUs, it is similarly defined as a
// forward C++ class declaration, because clang will not allow the type
// "NSAutoreleasePool" in such TUs. Finally, in Manual Retain Release (MRR)
// Objective-C TUs, it is a type alias for NSAutoreleasePool. In all cases, a
// method that takes or returns an NSAutoreleasePool* can use
// AutoreleasePoolType* instead.
#if !defined(__OBJC__) || __has_feature(objc_arc)
class AutoreleasePoolType;
#else   // !defined(__OBJC__) || __has_feature(objc_arc)
typedef NSAutoreleasePool AutoreleasePoolType;
#endif  // !defined(__OBJC__) || __has_feature(objc_arc)

class BASE_EXPORT MessagePumpCFRunLoopBase : public MessagePump {
 public:
  enum class LudicrousSlackSetting : uint8_t {
    kLudicrousSlackUninitialized,
    kLudicrousSlackOff,
    kLudicrousSlackOn,
    kLudicrousSlackSuspended,
  };

  MessagePumpCFRunLoopBase(const MessagePumpCFRunLoopBase&) = delete;
  MessagePumpCFRunLoopBase& operator=(const MessagePumpCFRunLoopBase&) = delete;

  // MessagePump:
  void Run(Delegate* delegate) override;
  void Quit() override;
  void ScheduleWork() override;
  void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;
  void SetTimerSlack(TimerSlack timer_slack) override;

#if defined(OS_IOS)
  // Some iOS message pumps do not support calling |Run()| to spin the main
  // message loop directly.  Instead, call |Attach()| to set up a delegate, then
  // |Detach()| before destroying the message pump.  These methods do nothing if
  // the message pump supports calling |Run()| and |Quit()|.
  // 一些 iOS 消息泵不支持调用 |Run()| 直接旋转主消息循环。相反，调用 |Attach()|
  // 设置委托，然后|Detach()| 在销毁消息泵之前。 如果消息泵支持调用 |Run()|，
  // 这些方法什么也不做。 和|退出()|。
  virtual void Attach(Delegate* delegate);
  virtual void Detach();
#endif  // OS_IOS

  // Exposed for testing.
  LudicrousSlackSetting GetLudicrousSlackStateForTesting() const {
    return GetLudicrousSlackState();
  }

 protected:
  // Needs access to CreateAutoreleasePool.
  friend class MessagePumpScopedAutoreleasePool;
  friend class TestMessagePumpCFRunLoopBase;

  // Tasks will be pumped in the run loop modes described by
  // |initial_mode_mask|, which maps bits to the index of an internal array of
  // run loop mode identifiers.
  explicit MessagePumpCFRunLoopBase(int initial_mode_mask);
  ~MessagePumpCFRunLoopBase() override;

  // Subclasses should implement the work they need to do in MessagePump::Run
  // in the DoRun method.  MessagePumpCFRunLoopBase::Run calls DoRun directly.
  // This arrangement is used because MessagePumpCFRunLoopBase needs to set
  // up and tear down things before and after the "meat" of DoRun.
  virtual void DoRun(Delegate* delegate) = 0;

  // Similar to DoRun, this allows subclasses to perform custom handling when
  // quitting a run loop. Return true if the quit took effect immediately;
  // otherwise call OnDidQuit() when the quit is actually applied (e.g., a
  // nested native runloop exited).
  virtual bool DoQuit() = 0;

  // Should be called by subclasses to signal when a deferred quit takes place.
  void OnDidQuit();

  // Accessors for private data members to be used by subclasses.
  // CFRunLoop实例，Mac/iOS底层消息循环机制
  CFRunLoopRef run_loop() const { return run_loop_; }
  int nesting_level() const { return nesting_level_; }
  int run_nesting_level() const { return run_nesting_level_; }
  bool keep_running() const { return keep_running_; }

  // Sets this pump's delegate.  Signals the appropriate sources if
  // |delegateless_work_| is true.  |delegate| can be NULL.
  void SetDelegate(Delegate* delegate);

  // Return an autorelease pool to wrap around any work being performed.
  // In some cases, CreateAutoreleasePool may return nil intentionally to
  // preventing an autorelease pool from being created, allowing any
  // objects autoreleased by work to fall into the current autorelease pool.
  virtual AutoreleasePoolType* CreateAutoreleasePool();

  // Enable and disable entries in |enabled_modes_| to match |mode_mask|.
  void SetModeMask(int mode_mask);

  // Get the current mode mask from |enabled_modes_|.
  int GetModeMask() const;

 private:
  class ScopedModeEnabler;

  // The maximum number of run loop modes that can be monitored.
  static constexpr int kNumModes = 4;

  // Returns the current ludicrous slack state, which implies reading both the
  // feature flag and the suspension state.
  LudicrousSlackSetting GetLudicrousSlackState() const;

  // All sources of delayed work scheduling converge to this, using TimeDelta
  // avoids querying Now() for key callers.
  void ScheduleDelayedWorkImpl(TimeDelta delta);

  // Timer callback scheduled by ScheduleDelayedWork.  This does not do any
  // work, but it signals |work_source_| so that delayed work can be performed
  // within the appropriate priority constraints.
  static void RunDelayedWorkTimer(CFRunLoopTimerRef timer, void* info);

  // Perform highest-priority work.  This is associated with |work_source_|
  // signalled by ScheduleWork or RunDelayedWorkTimer.  The static method calls
  // the instance method; the instance method returns true if it resignalled
  // |work_source_| to be called again from the loop.
  static void RunWorkSource(void* info);
  bool RunWork();

  // Perform idle-priority work.  This is normally called by PreWaitObserver,
  // but is also associated with |idle_work_source_|.  When this function
  // actually does perform idle work, it will resignal that source.  The
  // static method calls the instance method.
  static void RunIdleWorkSource(void* info);
  void RunIdleWork();

  // Perform work that may have been deferred because it was not runnable
  // within a nested run loop.  This is associated with
  // |nesting_deferred_work_source_| and is signalled by
  // MaybeScheduleNestingDeferredWork when returning from a nested loop,
  // so that an outer loop will be able to perform the necessary tasks if it
  // permits nestable tasks.
  static void RunNestingDeferredWorkSource(void* info);
  void RunNestingDeferredWork();

  // Called before the run loop goes to sleep to notify delegate.
  void BeforeWait();

  // Schedules possible nesting-deferred work to be processed before the run
  // loop goes to sleep, exits, or begins processing sources at the top of its
  // loop.  If this function detects that a nested loop had run since the
  // previous attempt to schedule nesting-deferred work, it will schedule a
  // call to RunNestingDeferredWorkSource.
  void MaybeScheduleNestingDeferredWork();

  // Observer callback responsible for performing idle-priority work, before
  // the run loop goes to sleep.  Associated with |pre_wait_observer_|.
  static void PreWaitObserver(CFRunLoopObserverRef observer,
                              CFRunLoopActivity activity, void* info);

  // Observer callback called before the run loop processes any sources.
  // Associated with |pre_source_observer_|.
  static void PreSourceObserver(CFRunLoopObserverRef observer,
                                CFRunLoopActivity activity, void* info);

  // Observer callback called when the run loop starts and stops, at the
  // beginning and end of calls to CFRunLoopRun.  This is used to maintain
  // |nesting_level_|.  Associated with |enter_exit_observer_|.
  static void EnterExitObserver(CFRunLoopObserverRef observer,
                                CFRunLoopActivity activity, void* info);

  // Called by EnterExitObserver after performing maintenance on
  // |nesting_level_|. This allows subclasses an opportunity to perform
  // additional processing on the basis of run loops starting and stopping.
  virtual void EnterExitRunLoop(CFRunLoopActivity activity);

  // The thread's run loop.
  CFRunLoopRef run_loop_; // iOS的CFRuntime实例

  // The enabled modes. Posted tasks may run in any non-null entry.
  std::unique_ptr<ScopedModeEnabler> enabled_modes_[kNumModes];

  // The timer, sources, and observers are described above alongside their
  // callbacks.
  CFRunLoopTimerRef delayed_work_timer_;

  // CFRunLoopSourceRef是产生事件的地方。Source包括Source0和Source1两个版本.
  // Source0：主要由应用程序管理，它并不能主动触发事件。使用时，你需要先调用
  // CFRunLoopSourceSignal(source)，将这个Source标记为待处理，然后手动调用
  // CFRunLoopWakeUp(runloop)来唤醒RunLoop，让其处理这个事件。通常我们使用
  // 的也是Source0事件。
  // Source1：主要由于RunLoop和kernel进行管理。包含了一个mach_port和一个回调
  // （函数指针），被用于通过内核和其他线程相互发送消息。这种Source能主动唤醒
  // RunLoop的线程。
  CFRunLoopSourceRef work_source_;
  CFRunLoopSourceRef idle_work_source_;
  CFRunLoopSourceRef nesting_deferred_work_source_;

  // 观察者，主要用于观察RunLoop的状态变化，以便在不同状态时做一些操作
  CFRunLoopObserverRef pre_wait_observer_;
  CFRunLoopObserverRef pre_source_observer_;
  CFRunLoopObserverRef enter_exit_observer_;

  // (weak) Delegate passed as an argument to the innermost Run call.
  Delegate* delegate_;

  base::TimerSlack timer_slack_;

  // Cache the ludicrous slack setting.
  LudicrousSlackSetting ludicrous_slack_setting_ =
      LudicrousSlackSetting::kLudicrousSlackUninitialized;

  // The recursion depth of the currently-executing CFRunLoopRun loop on the
  // run loop's thread.  0 if no run loops are running inside of whatever scope
  // the object was created in.
  int nesting_level_;

  // The recursion depth (calculated in the same way as |nesting_level_|) of the
  // innermost executing CFRunLoopRun loop started by a call to Run.
  int run_nesting_level_;

  // The deepest (numerically highest) recursion depth encountered since the
  // most recent attempt to run nesting-deferred work.
  int deepest_nesting_level_;

  // Whether we should continue running application tasks. Set to false when
  // Quit() is called for the innermost run loop.
  bool keep_running_;

  // "Delegateless" work flags are set when work is ready to be performed but
  // must wait until a delegate is available to process it.  This can happen
  // when a MessagePumpCFRunLoopBase is instantiated and work arrives without
  // any call to Run on the stack.  The Run method will check for delegateless
  // work on entry and redispatch it as needed once a delegate is available.
  bool delegateless_work_;
  bool delegateless_idle_work_;
};

class BASE_EXPORT MessagePumpCFRunLoop : public MessagePumpCFRunLoopBase {
 public:
  MessagePumpCFRunLoop();

  MessagePumpCFRunLoop(const MessagePumpCFRunLoop&) = delete;
  MessagePumpCFRunLoop& operator=(const MessagePumpCFRunLoop&) = delete;

  ~MessagePumpCFRunLoop() override;

  void DoRun(Delegate* delegate) override;
  bool DoQuit() override;

 private:
  void EnterExitRunLoop(CFRunLoopActivity activity) override;

  // True if Quit is called to stop the innermost MessagePump
  // (|innermost_quittable_|) but some other CFRunLoopRun loop
  // (|nesting_level_|) is running inside the MessagePump's innermost Run call.
  bool quit_pending_;
};

/**
 * @brief
 * NSRunLoop是Cocoa框架中的类，与之对应，在Core Fundation中是CFRunLoopRef类,
 * 这两者的区别是前者不是线程安全的，而后者是线程安全的。
 */
class BASE_EXPORT MessagePumpNSRunLoop : public MessagePumpCFRunLoopBase {
 public:
  MessagePumpNSRunLoop();

  MessagePumpNSRunLoop(const MessagePumpNSRunLoop&) = delete;
  MessagePumpNSRunLoop& operator=(const MessagePumpNSRunLoop&) = delete;

  ~MessagePumpNSRunLoop() override;

  void DoRun(Delegate* delegate) override;
  bool DoQuit() override;

 private:
  // A source that doesn't do anything but provide something signalable
  // attached to the run loop.  This source will be signalled when Quit
  // is called, to cause the loop to wake up so that it can stop.
  // 一个不做任何事情但提供附加到运行循环的信号的源。当 Quit 被调用时，该源将发出
  // 信号，以使循环唤醒，以便它可以停止。
  CFRunLoopSourceRef quit_source_; // 退出事件
};

#if defined(OS_IOS)
// This is a fake message pump.  It attaches sources to the main thread's
// CFRunLoop, so PostTask() will work, but it is unable to drive the loop
// directly, so calling Run() or Quit() are errors.
// 这是一个假消息泵。它将源附加到主线程的 CFRunLoop，因此 PostTask() 将起作用，
// 但它无法直接驱动循环，因此调用 Run() 或 Quit() 是错误的。
// iOS平台：支持UI事件的消息泵
class MessagePumpUIApplication : public MessagePumpCFRunLoopBase {
 public:
  MessagePumpUIApplication();

  MessagePumpUIApplication(const MessagePumpUIApplication&) = delete;
  MessagePumpUIApplication& operator=(const MessagePumpUIApplication&) = delete;

  ~MessagePumpUIApplication() override;
  void DoRun(Delegate* delegate) override;
  bool DoQuit() override;

  // MessagePumpCFRunLoopBase.
  // MessagePumpUIApplication can not spin the main message loop directly.
  // Instead, call |Attach()| to set up a delegate.  It is an error to call
  // |Run()|.
  void Attach(Delegate* delegate) override;
  void Detach() override;

 private:
  RunLoop* run_loop_;
};

#else

// While in scope, permits posted tasks to be run in private AppKit run loop
// modes that would otherwise make the UI unresponsive. E.g., menu fade out.
class BASE_EXPORT ScopedPumpMessagesInPrivateModes {
 public:
  ScopedPumpMessagesInPrivateModes();

  ScopedPumpMessagesInPrivateModes(const ScopedPumpMessagesInPrivateModes&) =
      delete;
  ScopedPumpMessagesInPrivateModes& operator=(
      const ScopedPumpMessagesInPrivateModes&) = delete;

  ~ScopedPumpMessagesInPrivateModes();

  int GetModeMaskForTest();
};

// Mac平台
class MessagePumpNSApplication : public MessagePumpCFRunLoopBase {
 public:
  MessagePumpNSApplication();

  MessagePumpNSApplication(const MessagePumpNSApplication&) = delete;
  MessagePumpNSApplication& operator=(const MessagePumpNSApplication&) = delete;

  ~MessagePumpNSApplication() override;

  void DoRun(Delegate* delegate) override;
  bool DoQuit() override;

 private:
  friend class ScopedPumpMessagesInPrivateModes;

  void EnterExitRunLoop(CFRunLoopActivity activity) override;

  // True if DoRun is managing its own run loop as opposed to letting
  // -[NSApplication run] handle it.  The outermost run loop in the application
  // is managed by -[NSApplication run], inner run loops are handled by a loop
  // in DoRun.
  bool running_own_loop_;

  // True if Quit() was called while a modal window was shown and needed to be
  // deferred.
  bool quit_pending_;
};

/**
 * @brief Mac平台处理UI事件的消息泵
 */
class MessagePumpCrApplication : public MessagePumpNSApplication {
 public:
  MessagePumpCrApplication();

  MessagePumpCrApplication(const MessagePumpCrApplication&) = delete;
  MessagePumpCrApplication& operator=(const MessagePumpCrApplication&) = delete;

  ~MessagePumpCrApplication() override;

 protected:
  // Returns nil if NSApp is currently in the middle of calling
  // -sendEvent.  Requires NSApp implementing CrAppProtocol.
  AutoreleasePoolType* CreateAutoreleasePool() override;
};
#endif  // !defined(OS_IOS)

class BASE_EXPORT MessagePumpMac {
 public:
  MessagePumpMac() = delete;
  MessagePumpMac(const MessagePumpMac&) = delete;
  MessagePumpMac& operator=(const MessagePumpMac&) = delete;

  // If not on the main thread, returns a new instance of
  // MessagePumpNSRunLoop.
  //
  // On the main thread, if NSApp exists and conforms to
  // CrAppProtocol, creates an instances of MessagePumpCrApplication.
  //
  // Otherwise creates an instance of MessagePumpNSApplication using a
  // default NSApplication.
  static std::unique_ptr<MessagePump> Create();

#if !defined(OS_IOS)
  // If a pump is created before the required CrAppProtocol is
  // created, the wrong MessagePump subclass could be used.
  // UsingCrApp() returns false if the message pump was created before
  // NSApp was initialized, or if NSApp does not implement
  // CrAppProtocol.  NSApp must be initialized before calling.
  static bool UsingCrApp();

  // Wrapper to query -[NSApp isHandlingSendEvent] from C++ code.
  // Requires NSApp to implement CrAppProtocol.
  static bool IsHandlingSendEvent();
#endif  // !defined(OS_IOS)
};

// Tasks posted to the message loop are posted under this mode, as well
// as kCFRunLoopCommonModes.
extern const CFStringRef BASE_EXPORT kMessageLoopExclusiveRunLoopMode;

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_MAC_H_
