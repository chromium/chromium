// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_CURRENT_THREAD_H_
#define BASE_TASK_CURRENT_THREAD_H_

#include <ostream>

#include "base/base_export.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/pending_task.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/task_time_observer.h"
#include "base/task/task_observer.h"
#include "build/build_config.h"

namespace web {
class WebTaskEnvironment;
}

namespace base {

namespace sequence_manager {
namespace internal {
class SequenceManagerImpl;
}
}  // namespace sequence_manager

// CurrentThread is a proxy to a subset of Task related APIs bound to the
// current thread
//
// Current(UI|IO)Thread is available statically through
// Current(UI|IO)Thread::Get() on threads that have registered as CurrentThread
// on this physical thread (e.g. by using SingleThreadTaskExecutor). APIs
// intended for all consumers on the thread should be on Current(UI|IO)Thread,
// while internal APIs might be on multiple internal classes (e.g.
// SequenceManager).
//
// Why: Historically MessageLoop would take care of everything related to event
// processing on a given thread. Nowadays that functionality is split among
// different classes. At that time MessageLoop::current() gave access to the
// full MessageLoop API, preventing both addition of powerful owner-only APIs as
// well as making it harder to remove callers of deprecated APIs (that need to
// stick around for a few owner-only use cases and re-accrue callers after
// cleanup per remaining publicly available).
//
// As such, many methods below are flagged as deprecated and should be removed
// once all static callers have been migrated.
class BASE_EXPORT CurrentThread {
 public:
  // CurrentThread is effectively just a disguised pointer and is fine to
  // copy/move around.
  CurrentThread(const CurrentThread& other) = default;
  CurrentThread(CurrentThread&& other) = default;
  CurrentThread& operator=(const CurrentThread& other) = default;

  bool operator==(const CurrentThread& other) const;

  // Returns a proxy object to interact with the Task related APIs for the
  // current thread. It must only be used on the thread it was obtained.
  static CurrentThread Get();

  // Return an empty CurrentThread. No methods should be called on this
  // object.
  static CurrentThread GetNull();

  // Returns true if the current thread is registered to expose CurrentThread
  // API. Prefer this to verifying the boolean value of Get() (so that Get() can
  // ultimately DCHECK it's only invoked when IsSet()).
  static bool IsSet();

  // Allow CurrentThread to be used like a pointer to support the many
  // callsites that used MessageLoop::current() that way when it was a
  // MessageLoop*.
  CurrentThread* operator->() { return this; }
  explicit operator bool() const { return !!current_; }

  // A DestructionObserver is notified when the current task execution
  // environment is being destroyed. These observers are notified prior to
  // CurrentThread::IsSet() being changed to return false. This gives interested
  // parties the chance to do final cleanup.
  //
  // NOTE: Any tasks posted to the current thread during this notification will
  // not be run. Instead, they will be deleted.
  //
  // Deprecation note: Prefer SequenceLocalStorageSlot<std::unique_ptr<Foo>> to
  // DestructionObserver to bind an object's lifetime to the current
  // thread/sequence.
  class BASE_EXPORT DestructionObserver {
   public:
    // TODO(https://crbug.com/891670): Rename to
    // WillDestroyCurrentTaskExecutionEnvironment
    virtual void WillDestroyCurrentMessageLoop() = 0;

   protected:
    virtual ~DestructionObserver() = default;
  };

  // Add a DestructionObserver, which will start receiving notifications
  // immediately.
  void AddDestructionObserver(DestructionObserver* destruction_observer);

  // Remove a DestructionObserver.  It is safe to call this method while a
  // DestructionObserver is receiving a notification callback.
  void RemoveDestructionObserver(DestructionObserver* destruction_observer);

  // Forwards to SequenceManager::SetTaskRunner().
  // DEPRECATED(https://crbug.com/825327): only owners of the SequenceManager
  // instance should replace its TaskRunner.
  void SetTaskRunner(scoped_refptr<SingleThreadTaskRunner> task_runner);

  // Forwards to SequenceManager::(Add|Remove)TaskObserver.
  // DEPRECATED(https://crbug.com/825327): only owners of the SequenceManager
  // instance should add task observers on it.
  void AddTaskObserver(TaskObserver* task_observer);
  void RemoveTaskObserver(TaskObserver* task_observer);

  void AddTaskTimeObserver(sequence_manager::TaskTimeObserver* task_observer);
  void RemoveTaskTimeObserver(
      sequence_manager::TaskTimeObserver* task_observer);

  // When this functionality is enabled, the queue time will be recorded for
  // posted tasks.
  void SetAddQueueTimeToTasks(bool enable);

  // Enables nested task processing in scope of an upcoming native message loop.
  // Some unwanted message loops may occur when using common controls or printer
  // functions. Hence, nested task processing is disabled by default to avoid
  // unplanned reentrancy. This re-enables it in cases where the stack is
  // reentrancy safe and processing nestable tasks is explicitly safe.
  //
  // For instance,
  // - The current thread is running a message loop.
  // - It receives a task #1 and executes it.
  // - The task #1 implicitly starts a nested message loop, like a MessageBox in
  //   the unit test. This can also be StartDoc or GetSaveFileName.
  // - The thread receives a task #2 before or while in this second message
  //   loop.
  // - With NestableTasksAllowed set to true, the task #2 will run right away.
  //   Otherwise, it will get executed right after task #1 completes at "thread
  //   message loop level".
  //
  // Use RunLoop::Type::kNestableTasksAllowed when nesting is triggered by the
  // application RunLoop rather than by native code.
  class BASE_EXPORT ScopedAllowApplicationTasksInNativeNestedLoop {
   public:
    ScopedAllowApplicationTasksInNativeNestedLoop();
    ~ScopedAllowApplicationTasksInNativeNestedLoop();

   private:
    sequence_manager::internal::SequenceManagerImpl* const sequence_manager_;
    const bool previous_state_;
  };

  // TODO(https://crbug.com/781352): Remove usage of this old class. Either
  // renaming it to ScopedAllowApplicationTasksInNativeNestedLoop when truly
  // native or migrating it to RunLoop::Type::kNestableTasksAllowed otherwise.
  using ScopedNestableTaskAllower =
      ScopedAllowApplicationTasksInNativeNestedLoop;

  // Returns true if nestable tasks are allowed on the current thread at this
  // time (i.e. if a nested loop would start from the callee's point in the
  // stack, would it be allowed to run application tasks).
  bool NestableTasksAllowed() const;

  // Returns true if this instance is bound to the current thread.
  bool IsBoundToCurrentThread() const;

  // Returns true if the current thread is idle (ignoring delayed tasks). This
  // is the same condition which triggers DoWork() to return false: i.e. out of
  // tasks which can be processed at the current run-level -- there might be
  // deferred non-nestable tasks remaining if currently in a nested run level.
  bool IsIdleForTesting();

 protected:
  explicit CurrentThread(
      sequence_manager::internal::SequenceManagerImpl* sequence_manager)
      : current_(sequence_manager) {}

  static sequence_manager::internal::SequenceManagerImpl*
  GetCurrentSequenceManagerImpl();

  friend class MessagePumpLibeventTest;
  friend class ScheduleWorkTest;
  friend class Thread;
  friend class sequence_manager::internal::SequenceManagerImpl;
  friend class MessageLoopTaskRunnerTest;
  friend class web::WebTaskEnvironment;

  sequence_manager::internal::SequenceManagerImpl* current_;
};

#if !defined(OS_NACL)

// UI extension of CurrentThread.
class BASE_EXPORT CurrentUIThread : public CurrentThread {
 public:
  // Returns an interface for the CurrentUIThread of the current thread.
  // Asserts that IsSet().
  static CurrentUIThread Get();

  // Returns true if the current thread is running a CurrentUIThread.
  static bool IsSet();

  CurrentUIThread* operator->() { return this; }

#if defined(USE_OZONE) && !defined(OS_FUCHSIA) && !defined(OS_WIN)
  static_assert(
      std::is_base_of<WatchableIOMessagePumpPosix, MessagePumpForUI>::value,
      "CurrentThreadForUI::WatchFileDescriptor is supported only"
      "by MessagePumpLibevent and MessagePumpGlib implementations.");
  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           MessagePumpForUI::Mode mode,
                           MessagePumpForUI::FdWatchController* controller,
                           MessagePumpForUI::FdWatcher* delegate);
#endif

#if defined(OS_IOS)
  // Forwards to SequenceManager::Attach().
  // TODO(https://crbug.com/825327): Plumb the actual SequenceManager* to
  // callers and remove ability to access this method from
  // CurrentUIThread.
  void Attach();
#endif

#if defined(OS_ANDROID)
  // Forwards to MessagePumpForUI::Abort().
  // TODO(https://crbug.com/825327): Plumb the actual MessagePumpForUI* to
  // callers and remove ability to access this method from
  // CurrentUIThread.
  void Abort();
#endif

#if defined(OS_WIN)
  void AddMessagePumpObserver(MessagePumpForUI::Observer* observer);
  void RemoveMessagePumpObserver(MessagePumpForUI::Observer* observer);
#endif

 private:
  explicit CurrentUIThread(
      sequence_manager::internal::SequenceManagerImpl* current)
      : CurrentThread(current) {}

  MessagePumpForUI* GetMessagePumpForUI() const;
};

#endif  // !defined(OS_NACL)

// ForIO extension of CurrentThread.
class BASE_EXPORT CurrentIOThread : public CurrentThread {
 public:
  // Returns an interface for the CurrentIOThread of the current thread.
  // Asserts that IsSet().
  static CurrentIOThread Get();

  // Returns true if the current thread is running a CurrentIOThread.
  static bool IsSet();

  CurrentIOThread* operator->() { return this; }

#if !defined(OS_NACL_SFI)

#if defined(OS_WIN)
  // Please see MessagePumpWin for definitions of these methods.
  HRESULT RegisterIOHandler(HANDLE file, MessagePumpForIO::IOHandler* handler);
  bool RegisterJobObject(HANDLE job, MessagePumpForIO::IOHandler* handler);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  // Please see WatchableIOMessagePumpPosix for definition.
  // Prefer base::FileDescriptorWatcher for non-critical IO.
  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           MessagePumpForIO::Mode mode,
                           MessagePumpForIO::FdWatchController* controller,
                           MessagePumpForIO::FdWatcher* delegate);
#endif  // defined(OS_WIN)

#if defined(OS_MAC)
  bool WatchMachReceivePort(
      mach_port_t port,
      MessagePumpForIO::MachPortWatchController* controller,
      MessagePumpForIO::MachPortWatcher* delegate);
#endif

#if defined(OS_FUCHSIA)
  // Additional watch API for native platform resources.
  bool WatchZxHandle(zx_handle_t handle,
                     bool persistent,
                     zx_signals_t signals,
                     MessagePumpForIO::ZxHandleWatchController* controller,
                     MessagePumpForIO::ZxHandleWatcher* delegate);
#endif  // defined(OS_FUCHSIA)

#endif  // !defined(OS_NACL_SFI)

 private:
  explicit CurrentIOThread(
      sequence_manager::internal::SequenceManagerImpl* current)
      : CurrentThread(current) {}

  MessagePumpForIO* GetMessagePumpForIO() const;
};

}  // namespace base

#endif  // BASE_TASK_CURRENT_THREAD_H_
