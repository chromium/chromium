// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_WIN_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_WIN_H_

#include <windows.h>

#include <atomic>
#include <list>
#include <memory>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/message_loop/message_pump.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/win/message_window.h"
#include "base/win/scoped_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

// MessagePumpWin serves as the base for specialized versions of the MessagePump
// for Windows. It provides basic functionality like handling of observers and
// controlling the lifetime of the message pump.
// MessagePumpWin 用作 Windows 专用版 MessagePump 的基础。它提供了基本功能，
// 例如处理 观察者 和控制 消息泵 的生命周期。
class BASE_EXPORT MessagePumpWin : public MessagePump {
 public:
  MessagePumpWin();
  ~MessagePumpWin() override;

  // MessagePump methods:
  void Run(Delegate* delegate) override;
  void Quit() override;

 protected:
  struct RunState {
    explicit RunState(Delegate* delegate_in) : delegate(delegate_in) {}

    Delegate* const delegate;

    // Used to flag that the current Run() invocation should return ASAP.
    // 用于标记当前的 Run() 调用应尽快返回。
    bool should_quit = false;

    // Set to true if this Run() is nested within another Run().
    // 如果此 Run() 嵌套在另一个 Run() 中，则设置为 true。
    bool is_nested = false;
  };

  virtual void DoRunLoop() = 0;

  // True iff:
  //   * MessagePumpForUI: there's a kMsgDoWork message pending in the Windows
  //     Message queue. i.e. when:
  //      a. The pump is about to wakeup from idle.
  //      b. The pump is about to enter a nested native loop and a
  //         ScopedNestableTaskAllower was instantiated to allow application
  //         tasks to execute in that nested loop (ScopedNestableTaskAllower
  //         invokes ScheduleWork()).
  //      c. While in a native (nested) loop : HandleWorkMessage() =>
  //         ProcessPumpReplacementMessage() invokes ScheduleWork() before
  //         processing a native message to guarantee this pump will get another
  //         time slice if it goes into native Windows code and enters a native
  //         nested loop. This is different from (b.) because we're not yet
  //         processing an application task at the current run level and
  //         therefore are expected to keep pumping application tasks without
  //         necessitating a ScopedNestableTaskAllower.
  //
  //   * MessagePumpforIO: there's a dummy IO completion item with |this| as an
  //     lpCompletionKey in the queue which is about to wakeup
  //     WaitForIOCompletion(). MessagePumpForIO doesn't support nesting so
  //     this is simpler than MessagePumpForUI.
  std::atomic_bool work_scheduled_{false};

  // State for the current invocation of Run(). null if not running.
  RunState* run_state_ = nullptr;

  THREAD_CHECKER(bound_thread_);
};

//-----------------------------------------------------------------------------
// MessagePumpForUI extends MessagePumpWin with methods that are particular to a
// MessageLoop instantiated with TYPE_UI.
// MessagePumpForUI 使用特定于使用 TYPE_UI 实例化的 MessageLoop 的方法扩展 MessagePumpWin。
//
// MessagePumpForUI implements a "traditional" Windows message pump. It contains
// a nearly infinite loop that peeks out messages, and then dispatches them.
// Intermixed with those peeks are callouts to DoWork. When there are no
// events to be serviced, this pump goes into a wait state. In most cases, this
// message pump handles all processing.
// MessagePumpForUI 实现了一个“传统的”Windows 消息泵。它包含一个几乎无限的循环，
// 可以窥探消息，然后分派它们。与这些偷看相混合的是对 DoWork 的标注。当没有要服务的
// 事件时，该泵进入等待状态。 在大多数情况下，此消息泵处理所有处理。
//
// However, when a task, or windows event, invokes on the stack a native dialog
// box or such, that window typically provides a bare bones (native?) message
// pump.  That bare-bones message pump generally supports little more than a
// peek of the Windows message queue, followed by a dispatch of the peeked
// message.  MessageLoop extends that bare-bones message pump to also service
// Tasks, at the cost of some complexity.
// 但是，当任务或 Windows 事件在堆栈上调用本机对话框等时，该窗口通常会提供基本的（本机？）
// 消息泵。该准系统消息泵通常只支持查看 Windows 消息队列，然后发送已查看的消息。
// MessageLoop 以一些复杂性为代价扩展了该准系统消息泵以服务于任务。
//
// The basic structure of the extension (referred to as a sub-pump) is that a
// special message, kMsgHaveWork, is repeatedly injected into the Windows
// Message queue.  Each time the kMsgHaveWork message is peeked, checks are made
// for an extended set of events, including the availability of Tasks to run.
// 扩展（称为子泵）的基本结构是一个特殊的消息，kMsgHaveWork，被重复注入到 Windows 消息队
// 列中。每次查看 kMsgHaveWork 消息时，都会检查一组扩展的事件，包括要运行的任务的可用性。
//
// After running a task, the special message kMsgHaveWork is again posted to the
// Windows Message queue, ensuring a future time slice for processing a future
// event.  To prevent flooding the Windows Message queue, care is taken to be
// sure that at most one kMsgHaveWork message is EVER pending in the Window's
// Message queue.
//
// There are a few additional complexities in this system where, when there are
// no Tasks to run, this otherwise infinite stream of messages which drives the
// sub-pump is halted.  The pump is automatically re-started when Tasks are
// queued.
//
// A second complexity is that the presence of this stream of posted tasks may
// prevent a bare-bones message pump from ever peeking a WM_PAINT or WM_TIMER.
// Such paint and timer events always give priority to a posted message, such as
// kMsgHaveWork messages.  As a result, care is taken to do some peeking in
// between the posting of each kMsgHaveWork message (i.e., after kMsgHaveWork is
// peeked, and before a replacement kMsgHaveWork is posted).
//
// NOTE: Although it may seem odd that messages are used to start and stop this
// flow (as opposed to signaling objects, etc.), it should be understood that
// the native message pump will *only* respond to messages.  As a result, it is
// an excellent choice.  It is also helpful that the starter messages that are
// placed in the queue when new task arrive also awakens DoRunLoop.
//
/**
 * @brief 创建了一个隐藏不可见窗口来处理需要在界面(UI)线程处理的消息，大体原理也就是需要执行
 * task的时候发送一个自定义的消息，当窗口接收到task的时候调用保存起来的回调函数，还有
 * 的是通过把回调放在消息结构体里面。
 * 利用Windows的隐式输入窗口，接收UI线程的事件，回调事件处理函数
 */
class BASE_EXPORT MessagePumpForUI : public MessagePumpWin {
 public:
  MessagePumpForUI();
  ~MessagePumpForUI() override;

  // MessagePump methods: 调度唤醒，继续执行消息循环，从proxy中获取消息
  void ScheduleWork() override;
  void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;

  // Make the MessagePumpForUI respond to WM_QUIT messages.
  void EnableWmQuit();

  // An observer interface to give the scheduler an opportunity to log
  // information about MSGs before and after they are dispatched.
  // Windows窗口的观察者
  class BASE_EXPORT Observer {
   public:
    // 在分发消息前调用
    virtual void WillDispatchMSG(const MSG& msg) = 0;
    // 在分发消息后调用
    virtual void DidDispatchMSG(const MSG& msg) = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  bool MessageCallback(UINT message,
                       WPARAM wparam,
                       LPARAM lparam,
                       LRESULT* result);

  // 真实执行消息循环的函数
  void DoRunLoop() override;

  NOINLINE void NOT_TAIL_CALLED
  WaitForWork(Delegate::NextWorkInfo next_work_info);

  void HandleWorkMessage();
  void HandleTimerMessage();
  void ScheduleNativeTimer(Delegate::NextWorkInfo next_work_info);
  void KillNativeTimer();
  bool ProcessNextWindowsMessage();
  bool ProcessMessageHelper(const MSG& msg);
  bool ProcessPumpReplacementMessage();

  // MessageWindow即是上面讲的封装 Message-Only “隐式输入窗口”的类
  // 通过这个字段间接利用Windows的 “隐式输入窗口”，接收消息，触发回调的。
  base::win::MessageWindow message_window_;

  // Whether MessagePumpForUI responds to WM_QUIT messages or not.
  // TODO(thestig): Remove when the Cloud Print Service goes away.
  bool enable_wm_quit_ = false;

  // Non-nullopt if there's currently a native timer installed. If so, it
  // indicates when the timer is set to fire and can be used to avoid setting
  // redundant timers.
  absl::optional<TimeTicks> installed_native_timer_;

  // This will become true when a native loop takes our kMsgHaveWork out of the
  // system queue. It will be reset to false whenever DoRunLoop regains control.
  // Used to decide whether ScheduleDelayedWork() should start a native timer.
  bool in_native_loop_ = false;

  // Windows窗口事件的观察者列表
  ObserverList<Observer>::Unchecked observers_;
};

//-----------------------------------------------------------------------------
// MessagePumpForIO extends MessagePumpWin with methods that are particular to a
// MessageLoop instantiated with TYPE_IO. This version of MessagePump does not
// deal with Windows mesagges, and instead has a Run loop based on Completion
// Ports so it is better suited for IO operations.
// 在Windows系统里，使用完成端口是高性能的方法之一，比如把完成端口使用到线程池和网络
// 服务器里。现在就通过线程池的方法来介绍怎么样使用完成端口，高性能的服务器以后再仔细
// 地介绍怎么样构造它。其实完成端口是一个队列，所有的线程都在等消息出现，如果队列里有
// 消息，就每个线程去获取一个消息执行它。先用函数CreateIoCompletionPort来创建一个
// 消息队列，然后使用GetQueuedCompletionStatus函数来从队列获取消息，使用函数
// PostQueuedCompletionStatus来向队列里发送消息。通过这三个函数就实现完成端口的
// 消息循环处理。
class BASE_EXPORT MessagePumpForIO : public MessagePumpWin {
 public:
  struct BASE_EXPORT IOContext {
    IOContext();
    OVERLAPPED overlapped;
  };

  // Clients interested in receiving OS notifications when asynchronous IO
  // operations complete should implement this interface and register themselves
  // with the message pump.
  //
  // Typical use #1:
  //   class MyFile : public IOHandler {
  //     MyFile() : IOHandler(FROM_HERE) {
  //       ...
  //       message_pump->RegisterIOHandler(file_, this);
  //     }
  //     // Plus some code to make sure that this destructor is not called
  //     // while there are pending IO operations.
  //     ~MyFile() {
  //     }
  //     virtual void OnIOCompleted(IOContext* context, DWORD bytes_transfered,
  //                                DWORD error) {
  //       ...
  //       delete context;
  //     }
  //     void DoSomeIo() {
  //       ...
  //       IOContext* context = new IOContext;
  //       ReadFile(file_, buffer, num_bytes, &read, &context);
  //     }
  //     HANDLE file_;
  //
  //
  // Typical use #2:
  // Same as the previous example, except that in order to deal with the
  // requirement stated for the destructor, the class calls WaitForIOCompletion
  // from the destructor to block until all IO finishes.
  //     ~MyFile() {
  //       while(pending_)
  //         message_pump->WaitForIOCompletion(INFINITE, this);
  //     }
  //
  class BASE_EXPORT IOHandler {
   public:
    explicit IOHandler(const Location& from_here);
    virtual ~IOHandler();

    IOHandler(const IOHandler&) = delete;
    IOHandler& operator=(const IOHandler&) = delete;

    // This will be called once the pending IO operation associated with
    // |context| completes. |error| is the Win32 error code of the IO operation
    // (ERROR_SUCCESS if there was no error). |bytes_transfered| will be zero
    // on error.
    virtual void OnIOCompleted(IOContext* context,
                               DWORD bytes_transfered,
                               DWORD error) = 0;

    const Location& io_handler_location() { return io_handler_location_; }

   private:
    const Location io_handler_location_;
  };

  MessagePumpForIO();
  ~MessagePumpForIO() override;

  // MessagePump methods:
  void ScheduleWork() override;
  void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;

  // Register the handler to be used when asynchronous IO for the given file
  // completes. The registration persists as long as |file_handle| is valid, so
  // |handler| must be valid as long as there is pending IO for the given file.
  HRESULT RegisterIOHandler(HANDLE file_handle, IOHandler* handler);

  // Register the handler to be used to process job events. The registration
  // persists as long as the job object is live, so |handler| must be valid
  // until the job object is destroyed. Returns true if the registration
  // succeeded, and false otherwise.
  bool RegisterJobObject(HANDLE job_handle, IOHandler* handler);

 private:
  struct IOItem {
    IOHandler* handler;
    IOContext* context;
    DWORD bytes_transfered;
    DWORD error;
  };

  void DoRunLoop() override;
  NOINLINE void NOT_TAIL_CALLED
  WaitForWork(Delegate::NextWorkInfo next_work_info);
  bool GetIOItem(DWORD timeout, IOItem* item);
  bool ProcessInternalIOItem(const IOItem& item);
  // Waits for the next IO completion for up to |timeout| milliseconds.
  // Return true if any IO operation completed, and false if the timeout
  // expired. If the completion port received any messages, the associated
  // handlers will have been invoked before returning from this code.
  bool WaitForIOCompletion(DWORD timeout);

  // The completion port associated with this thread.
  // 与此线程关联的完成端口。
  win::ScopedHandle port_;
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_WIN_H_
