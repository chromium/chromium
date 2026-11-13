// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_WIN_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_WIN_H_

#include <stdint.h>

#include <atomic>
#include <memory>
#include <optional>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/message_loop/message_pump.h"
#include "base/observer_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/win/message_window.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"

namespace base {

class MessagePumpForIOEventImpl;

// MessagePumpWin serves as the base for specialized versions of the MessagePump
// for Windows. It provides basic functionality like handling of observers and
// controlling the lifetime of the message pump.
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

    const raw_ptr<Delegate> delegate;

    // Used to flag that the current Run() invocation should return ASAP.
    bool should_quit = false;

    // Set to true if this Run() is nested within another Run().
    bool is_nested = false;
  };

  virtual void DoRunLoop() = 0;

  // True iff:
  //   * MessagePumpForUI: there's a kMsgDoWork message pending in the Windows
  //     Message queue. i.e. when:
  //      a. The pump is about to enter a nested native loop and a
  //         `ScopedAllowApplicationTasksInNativeNestedLoop` was instantiated to
  //         allow application tasks to execute in that nested loop
  //         (`ScopedAllowApplicationTasksInNativeNestedLoop` invokes
  //         ScheduleWork()).
  //      b. While in a native (nested) loop : HandleWorkMessage() =>
  //         ProcessPumpReplacementMessage() invokes ScheduleWork() before
  //         processing a native message to guarantee this pump will get another
  //         time slice if it goes into native Windows code and enters a native
  //         nested loop. This is different from (b.) because we're not yet
  //         processing an application task at the current run level and
  //         therefore are expected to keep pumping application tasks without
  //         necessitating a `ScopedAllowApplicationTasksInNativeNestedLoop`.
  //
  //   * MessagePumpForIOCompletionPortImpl: there's a dummy IO completion item
  //     with `this` as an lpCompletionKey in the queue about to wake up
  //     WaitForIOCompletion(). MessagePumpForIO doesn't support nesting so
  //     this is simpler than MessagePumpForUI.
  //   * MessagePumpForIOEventImpl: the task-wakeup event is signaled or about
  //     to be signaled by ScheduleWork().
  //
  // Note that this should not be used for memory ordering. It is accessed via
  // `memory_order_relaxed` in all cases.
  std::atomic_bool native_msg_scheduled_{false};

  raw_ptr<RunState> run_state_ = nullptr;

  THREAD_CHECKER(bound_thread_);
};

//-----------------------------------------------------------------------------
// MessagePumpForUI extends MessagePumpWin with methods that are particular to a
// MessageLoop instantiated with TYPE_UI.
//
// MessagePumpForUI implements a "traditional" Windows message pump. It contains
// a nearly infinite loop that peeks out messages, and then dispatches them.
// Intermixed with those peeks are callouts to DoWork. When there are no
// events to be serviced, this pump goes into a wait state. In most cases, this
// message pump handles all processing.
//
// However, when a task, or windows event, invokes on the stack a native dialog
// box or such, that window typically provides a bare bones (native?) message
// pump.  That bare-bones message pump generally supports little more than a
// peek of the Windows message queue, followed by a dispatch of the peeked
// message.  MessageLoop extends that bare-bones message pump to also service
// Tasks, at the cost of some complexity.
//
// The basic structure of the extension (referred to as a sub-pump) is that a
// special message, kMsgHaveWork, is repeatedly injected into the Windows
// Message queue.  Each time the kMsgHaveWork message is peeked, checks are made
// for an extended set of events, including the availability of Tasks to run.
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
class BASE_EXPORT MessagePumpForUI : public MessagePumpWin {
 public:
  MessagePumpForUI();
  ~MessagePumpForUI() override;

  // MessagePump methods:
  void ScheduleWork() override;
  void ScheduleDelayedWork(
      const Delegate::NextWorkInfo& next_work_info) override;
  bool HandleNestedNativeLoopWithApplicationTasks(
      bool application_tasks_desired) override;

  // An observer interface to give the scheduler an opportunity to log
  // information about MSGs before and after they are dispatched.
  // There is at most one observer at a time.
  class BASE_EXPORT NativeEventObserver {
   public:
    virtual void WillRunNativeEvent(uintptr_t identifier) = 0;
    virtual void DidRunNativeEvent(uintptr_t identifier) = 0;
  };

  void RegisterNativeEventObserver(NativeEventObserver* observer);
  void UnregisterNativeEventObserver(NativeEventObserver* observer);

  // For testing only, allows overriding the current observer.
  // Returns the previous observer.
  NativeEventObserver* ResetNativeEventObserverForTesting(
      NativeEventObserver* observer);

 private:
  bool MessageCallback(UINT message,
                       WPARAM wparam,
                       LPARAM lparam,
                       LRESULT* result);
  void DoRunLoop() override;
  NOINLINE NOT_TAIL_CALLED void WaitForWork(
      Delegate::NextWorkInfo next_work_info);
  void HandleWorkMessage();
  void HandleTimerMessage();
  void ScheduleNativeTimer(Delegate::NextWorkInfo next_work_info);
  void KillNativeTimer();
  bool ProcessNextWindowsMessage();
  bool ProcessMessageHelper(const MSG& msg);
  bool ProcessPumpReplacementMessage();

  base::win::MessageWindow message_window_;

  // Non-nullopt if there's currently a native timer installed. If so, it
  // indicates when the timer is set to fire and can be used to avoid setting
  // redundant timers.
  std::optional<TimeTicks> installed_native_timer_;

  // This is used to wake up the pump.
  WaitableEvent event_{WaitableEvent::ResetPolicy::AUTOMATIC};

  // This is set when HandleNestedNativeLoopWithApplicationTasks(true) was
  // called (when a `ScopedAllowApplicationTasksInNativeNestedLoop` is
  // instantiated).
  //
  // When running with `event_`, switches to pumping
  // `kMsgHaveWork` MSGs when there are application tasks to be done during
  // native runloops. In this state, ScheduleDelayedWork() will start a native
  // timer.
  //
  // It is reset when:
  //   - DoRunLoop() gets control back after ProcessNextWindowsMessage().
  //   - HandleNestedNativeLoopWithApplicationTasks(false) is called.
  bool in_nested_native_loop_with_application_tasks_ = false;

  enum class WakeupState {
    kApplicationTask,
    kNative,
    kRunning,
    kInactive,
  };
  // Used to keep track of what the pump knows about the state of its work
  // sources at wakeup. Its value is `kInactive` at construction, but set to
  // `kRunning` on entry to DoRunLoop().
  WakeupState wakeup_state_ = WakeupState::kInactive;

  raw_ptr<NativeEventObserver> native_event_observer_ = nullptr;

  uintptr_t next_peek_message_event_id_ = reinterpret_cast<uintptr_t>(this);
};

//-----------------------------------------------------------------------------
// MessagePumpForIO extends MessagePumpWin with methods that are particular to a
// MessageLoop instantiated with TYPE_IO. Completion ports are the default.
// Renderer startup selects completion-event watches when its event-pump feature
// is enabled, to support Windows Basic Processes.
//
class BASE_EXPORT MessagePumpForIO : public MessagePumpWin {
 public:
  class BASE_EXPORT IOContext {
   public:
    IOContext();
    ~IOContext();

    OVERLAPPED* GetOverlapped();

   private:
    // Hack: This header needs to be pulled in by files that should not
    // `#include <windows.h>`, yet wants to store an `OVERLAPPED` inline.
    // We can't simply define `OVERLAPPED` ourselves, or the compiler will
    // complain about type redefinitions in files that _do_ see the real
    // definition. Instead, define an identical struct, but use it only to
    // align/size storage that we will construct a real `OVERLAPPED` in in the
    // constructor.
    struct Sizer {
      ULONG_PTR Internal;
      ULONG_PTR InternalHigh;
      union {
        struct {
          DWORD Offset;
          DWORD OffsetHigh;
        } DUMMYSTRUCTNAME;
        PVOID Pointer;
      } DUMMYUNIONNAME;
      HANDLE hEvent;
    };

    alignas(Sizer) unsigned char storage_[sizeof(Sizer)];
  };

  // Clients interested in receiving OS notifications when asynchronous IO
  // operations complete use IOHandler with completion-port pumps or
  // IOEventHandler with event-based pumps.
  //
  // Completion-port example:
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
  //       ReadFile(file_, buffer, num_bytes, &read, context->GetOverlapped());
  //     }
  //     HANDLE file_;
  //   };
  //
  // Completion-event example (all methods run on the I/O thread):
  //   class MyEventFile : public IOEventHandler {
  //    public:
  //     explicit MyEventFile(win::ScopedHandle event_handle)
  //         : IOEventHandler(FROM_HERE), watch_(std::move(event_handle)) {
  //       ...  // Initialize file_ for overlapped I/O.
  //       CHECK(message_pump->SupportsIOEvents());
  //       CHECK(message_pump->WatchIOEvent(&watch_, this));
  //       context_.GetOverlapped()->hEvent = watch_.event_handle();
  //     }
  //     // Do not destroy this object while an operation is pending.
  //     ~MyEventFile() {
  //       DCHECK(!pending_);
  //       watch_.StopWatching();
  //     }
  //     void DoSomeIo() {
  //       DCHECK(!pending_);
  //       const BOOL ok = ReadFile(file_, buffer, num_bytes, nullptr,
  //                                context_.GetOverlapped());
  //       PCHECK(ok || GetLastError() == ERROR_IO_PENDING);
  //       pending_ = true;
  //     }
  //     void OnIOEventSignaled(IOEventWatchController* controller) override {
  //       DCHECK_EQ(controller, &watch_);
  //       DCHECK(pending_);
  //       DWORD bytes_transferred = 0;
  //       // This example keeps file_ valid until completion.
  //       const BOOL ok = GetOverlappedResult(
  //           file_, context_.GetOverlapped(), &bytes_transferred, FALSE);
  //       const DWORD error = ok ? ERROR_SUCCESS : GetLastError();
  //       pending_ = false;
  //       ...  // Handle bytes_transferred and error.
  //     }
  //    private:
  //     HANDLE file_;
  //     IOContext context_;
  //     IOEventWatchController watch_;
  //     bool pending_ = false;
  //   };
  //
  // Event-based pumps do not use RegisterIOHandler(). The wait consumes the
  // event signal before notification. Use a separate context and controller for
  // each concurrently pending operation.
  //
  // Keep handlers, contexts, buffers, and event controllers alive until pending
  // I/O completes; destroying the pump or stopping a watch does not establish
  // completion.
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
    //
    // |context| is the raw OVERLAPPED* from the completion packet. It does not
    // necessarily belong to this handler, or even point into this process:
    // another handle to the same file object, possibly in another process, may
    // have started the IO, and job notifications (see RegisterJobObject())
    // carry a message-specific value such as a process id instead. Do not
    // dereference it without checking that it is one of this handler's.
    virtual void OnIOCompleted(IOContext* context,
                               DWORD bytes_transfered,
                               DWORD error) = 0;

    const Location& io_handler_location() { return io_handler_location_; }

   private:
    const Location io_handler_location_;
  };

  class IOEventHandler;

  // Owns a supplied completion event and its watch, not the file handle or
  // OVERLAPPED.
  // The handler must outlive its watch. The controller and operation state
  // must outlive pending I/O using this event.
  // Watching and stopping must happen on the I/O thread. After pump teardown,
  // the detached controller can be destroyed on any thread.
  // Stopping a watch does not cancel or drain its outstanding I/O.
  class BASE_EXPORT IOEventWatchController {
   public:
    // Takes ownership of a valid, initially unsignaled auto-reset event with
    // SYNCHRONIZE and EVENT_MODIFY_STATE access. Callers may supply a brokered
    // handle when local event creation is unavailable.
    explicit IOEventWatchController(win::ScopedHandle event_handle);
    ~IOEventWatchController();

    IOEventWatchController(const IOEventWatchController&) = delete;
    IOEventWatchController& operator=(const IOEventWatchController&) = delete;

    HANDLE event_handle() const;
    void StopWatching();

   private:
    friend class MessagePumpForIOEventImpl;

    WaitableEvent event_;
    raw_ptr<MessagePumpForIOEventImpl> pump_ = nullptr;
    raw_ptr<IOEventHandler> handler_ = nullptr;
  };

  class BASE_EXPORT IOEventHandler {
   public:
    explicit IOEventHandler(const Location& from_here);

    // The pump is the only waiter and consumes the signal before notification.
    // While its file handle is valid, the consumer can retrieve the I/O result
    // with GetOverlappedResult(..., FALSE). Do not use bWait=TRUE with these
    // events: the pump has consumed the completion signal, so waiting again
    // risks hanging. During shutdown, the completed operation can instead be
    // retired without querying its result.
    virtual void OnIOEventSignaled(IOEventWatchController* controller) = 0;

    const Location& io_handler_location() const LIFETIME_BOUND {
      return io_handler_location_;
    }

   protected:
    virtual ~IOEventHandler() = default;

   private:
    const Location io_handler_location_;
  };

  // Creates an IOCP pump unless event-based pumps were enabled at startup or
  // selected by the testing override.
  static std::unique_ptr<MessagePumpForIO> Create();

  // Selects event-based pumps for the process. Must be called during startup,
  // before any I/O pumps are created.
  static void EnableEventBasedPump();

  // Overrides subsequent Create() calls to use events when enabled. Returns
  // the previous override, which must be restored in reverse nesting order.
  // Setting or restoring the override must not overlap pump creation.
  // Existing pumps and the production selection are unaffected.
  static bool SetEventBasedPumpForTesting(bool enabled);

  ~MessagePumpForIO() override;

  // MessagePump methods:
  void ScheduleWork() final;
  void ScheduleDelayedWork(
      const Delegate::NextWorkInfo& next_work_info) override;

  // Returns whether this pump supports completion-event watches.
  virtual bool SupportsIOEvents() const;

  // Adds the event in `controller` to the watched I/O events, notifying
  // `handler` when it is signaled. The controller must remain alive until the
  // watch is stopped or the pump is destroyed; the handler must outlive the
  // watch. The controller and operation state must also outlive any pending
  // I/O using the event, even after the watch stops. Returns false if the wait
  // set is full. Only event-based pumps support this operation.
  [[nodiscard]] virtual bool WatchIOEvent(IOEventWatchController* controller,
                                          IOEventHandler* handler);

  // Register the handler to be used when asynchronous IO for the given file
  // completes. The registration is attached to the kernel file object behind
  // |file_handle| and lasts as long as that object does. Other handles or
  // pending IO, possibly from another process, can keep it alive after
  // |file_handle| is closed, so |handler| must stay valid as long as any IO on
  // the file can complete. Returns true iff the registration succeeds.
  // Requires a completion-port pump (!SupportsIOEvents()); calling this on an
  // event-based pump is a contract violation, not a registration failure.
  [[nodiscard]] virtual bool RegisterIOHandler(HANDLE file_handle,
                                               IOHandler* handler) = 0;

  // Register the handler to be used to process job events. The registration
  // persists as long as the job object is live, so |handler| must be valid
  // until the job object is destroyed. Returns true if the registration
  // succeeded, and false otherwise.
  // Requires a completion-port pump (!SupportsIOEvents()); calling this on an
  // event-based pump is a contract violation, not a registration failure.
  virtual bool RegisterJobObject(HANDLE job_handle, IOHandler* handler) = 0;

 protected:
  MessagePumpForIO();

  // Waits up to `timeout` milliseconds for a completion or task wakeup.
  // Returns true if a notification was handled.
  virtual bool WaitForIOCompletion(DWORD timeout) = 0;

 private:
  // Signals native task work from any thread. Returns false on failure.
  virtual bool ScheduleNativeWork() = 0;

  void DoRunLoop() final;
  NOINLINE NOT_TAIL_CALLED void WaitForWork(
      Delegate::NextWorkInfo next_work_info);
};

class BASE_EXPORT MessagePumpForIOCompletionPortImpl : public MessagePumpForIO {
 public:
  MessagePumpForIOCompletionPortImpl();
  ~MessagePumpForIOCompletionPortImpl() override;

  bool RegisterIOHandler(HANDLE file_handle, IOHandler* handler) override;
  bool RegisterJobObject(HANDLE job_handle, IOHandler* handler) override;

 private:
  struct IOItem {
    STACK_ALLOCATED();

   public:
    raw_ptr<IOHandler> handler;
    // Not necessarily a valid pointer in this process.
    IOContext* context = nullptr;
    DWORD bytes_transfered = 0;
    DWORD error = 0;
  };

  bool ScheduleNativeWork() override;
  bool WaitForIOCompletion(DWORD timeout) override;
  bool GetIOItem(DWORD timeout, IOItem* item);
  bool ProcessInternalIOItem(const IOItem& item);

  // The completion port associated with this thread.
  win::ScopedHandle port_;
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_WIN_H_
