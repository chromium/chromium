// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/current_thread.h"

#include "base/bind.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/base_tracing.h"

namespace base {

//------------------------------------------------------------------------------
// CurrentThread

// static
sequence_manager::internal::SequenceManagerImpl*
CurrentThread::GetCurrentSequenceManagerImpl() {
  return sequence_manager::internal::SequenceManagerImpl::GetCurrent();
}

// static
CurrentThread CurrentThread::Get() {
  return CurrentThread(GetCurrentSequenceManagerImpl());
}

// static
CurrentThread CurrentThread::GetNull() {
  return CurrentThread(nullptr);
}

// static
bool CurrentThread::IsSet() {
  return !!GetCurrentSequenceManagerImpl();
}

void CurrentThread::AddDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK(current_->IsBoundToCurrentThread());
  current_->AddDestructionObserver(destruction_observer);
}

void CurrentThread::RemoveDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK(current_->IsBoundToCurrentThread());
  current_->RemoveDestructionObserver(destruction_observer);
}

void CurrentThread::SetTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  DCHECK(current_->IsBoundToCurrentThread());
  current_->SetTaskRunner(std::move(task_runner));
}

bool CurrentThread::IsBoundToCurrentThread() const {
  return current_ == GetCurrentSequenceManagerImpl();
}

bool CurrentThread::IsIdleForTesting() {
  DCHECK(current_->IsBoundToCurrentThread());
  return current_->IsIdleForTesting();
}

void CurrentThread::AddTaskObserver(TaskObserver* task_observer) {
  DCHECK(current_->IsBoundToCurrentThread());
  current_->AddTaskObserver(task_observer);
}

void CurrentThread::RemoveTaskObserver(TaskObserver* task_observer) {
  DCHECK(current_->IsBoundToCurrentThread());
  current_->RemoveTaskObserver(task_observer);
}

void CurrentThread::AddTaskTimeObserver(
    sequence_manager::TaskTimeObserver* task_observer) {
  DCHECK(current_->IsBoundToCurrentThread());
  current_->AddTaskTimeObserver(task_observer);
}

void CurrentThread::RemoveTaskTimeObserver(
    sequence_manager::TaskTimeObserver* task_observer) {
  DCHECK(current_->IsBoundToCurrentThread());
  current_->RemoveTaskTimeObserver(task_observer);
}

void CurrentThread::SetAddQueueTimeToTasks(bool enable) {
  DCHECK(current_->IsBoundToCurrentThread());
  current_->SetAddQueueTimeToTasks(enable);
}

CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop::
    ScopedAllowApplicationTasksInNativeNestedLoop()
    : sequence_manager_(GetCurrentSequenceManagerImpl()),
      previous_state_(sequence_manager_->IsTaskExecutionAllowed()) {
  TRACE_EVENT_BEGIN0("base", "ScopedNestableTaskAllower");
  sequence_manager_->SetTaskExecutionAllowed(true);
}

CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop::
    ~ScopedAllowApplicationTasksInNativeNestedLoop() {
  sequence_manager_->SetTaskExecutionAllowed(previous_state_);
  TRACE_EVENT_END0("base", "ScopedNestableTaskAllower");
}

bool CurrentThread::NestableTasksAllowed() const {
  return current_->IsTaskExecutionAllowed();
}

bool CurrentThread::operator==(const CurrentThread& other) const {
  return current_ == other.current_;
}

#if !defined(OS_NACL)

//------------------------------------------------------------------------------
// CurrentUIThread

// static
CurrentUIThread CurrentUIThread::Get() {
  auto* sequence_manager = GetCurrentSequenceManagerImpl();
  DCHECK(sequence_manager);
#if defined(OS_ANDROID)
  DCHECK(sequence_manager->IsType(MessagePumpType::UI) ||
         sequence_manager->IsType(MessagePumpType::JAVA));
#else   // defined(OS_ANDROID)
  DCHECK(sequence_manager->IsType(MessagePumpType::UI));
#endif  // defined(OS_ANDROID)
  return CurrentUIThread(sequence_manager);
}

// static
bool CurrentUIThread::IsSet() {
  sequence_manager::internal::SequenceManagerImpl* sequence_manager =
      GetCurrentSequenceManagerImpl();
  return sequence_manager &&
#if defined(OS_ANDROID)
         (sequence_manager->IsType(MessagePumpType::UI) ||
          sequence_manager->IsType(MessagePumpType::JAVA));
#else   // defined(OS_ANDROID)
         sequence_manager->IsType(MessagePumpType::UI);
#endif  // defined(OS_ANDROID)
}

MessagePumpForUI* CurrentUIThread::GetMessagePumpForUI() const {
  return static_cast<MessagePumpForUI*>(current_->GetMessagePump());
}

#if defined(USE_OZONE) && !defined(OS_FUCHSIA) && !defined(OS_WIN)
bool CurrentUIThread::WatchFileDescriptor(
    int fd,
    bool persistent,
    MessagePumpForUI::Mode mode,
    MessagePumpForUI::FdWatchController* controller,
    MessagePumpForUI::FdWatcher* delegate) {
  DCHECK(current_->IsBoundToCurrentThread());
  return GetMessagePumpForUI()->WatchFileDescriptor(fd, persistent, mode,
                                                    controller, delegate);
}
#endif

#if defined(OS_IOS)
void CurrentUIThread::Attach() {
  current_->AttachToMessagePump();
}
#endif  // defined(OS_IOS)

#if defined(OS_ANDROID)
void CurrentUIThread::Abort() {
  GetMessagePumpForUI()->Abort();
}
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
void CurrentUIThread::AddMessagePumpObserver(
    MessagePumpForUI::Observer* observer) {
  GetMessagePumpForUI()->AddObserver(observer);
}

void CurrentUIThread::RemoveMessagePumpObserver(
    MessagePumpForUI::Observer* observer) {
  GetMessagePumpForUI()->RemoveObserver(observer);
}
#endif  // defined(OS_WIN)

#endif  // !defined(OS_NACL)

//------------------------------------------------------------------------------
// CurrentIOThread

// static
CurrentIOThread CurrentIOThread::Get() {
  auto* sequence_manager = GetCurrentSequenceManagerImpl();
  DCHECK(sequence_manager);
  DCHECK(sequence_manager->IsType(MessagePumpType::IO));
  return CurrentIOThread(sequence_manager);
}

// static
bool CurrentIOThread::IsSet() {
  auto* sequence_manager = GetCurrentSequenceManagerImpl();
  return sequence_manager && sequence_manager->IsType(MessagePumpType::IO);
}

MessagePumpForIO* CurrentIOThread::GetMessagePumpForIO() const {
  return static_cast<MessagePumpForIO*>(current_->GetMessagePump());
}

#if !defined(OS_NACL_SFI)

#if defined(OS_WIN)
HRESULT CurrentIOThread::RegisterIOHandler(
    HANDLE file,
    MessagePumpForIO::IOHandler* handler) {
  DCHECK(current_->IsBoundToCurrentThread());
  return GetMessagePumpForIO()->RegisterIOHandler(file, handler);
}

bool CurrentIOThread::RegisterJobObject(HANDLE job,
                                        MessagePumpForIO::IOHandler* handler) {
  DCHECK(current_->IsBoundToCurrentThread());
  return GetMessagePumpForIO()->RegisterJobObject(job, handler);
}

#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
bool CurrentIOThread::WatchFileDescriptor(
    int fd,
    bool persistent,
    MessagePumpForIO::Mode mode,
    MessagePumpForIO::FdWatchController* controller,
    MessagePumpForIO::FdWatcher* delegate) {
  DCHECK(current_->IsBoundToCurrentThread());
  return GetMessagePumpForIO()->WatchFileDescriptor(fd, persistent, mode,
                                                    controller, delegate);
}
#endif  // defined(OS_WIN)

#if defined(OS_MAC)
bool CurrentIOThread::WatchMachReceivePort(
    mach_port_t port,
    MessagePumpForIO::MachPortWatchController* controller,
    MessagePumpForIO::MachPortWatcher* delegate) {
  DCHECK(current_->IsBoundToCurrentThread());
  return GetMessagePumpForIO()->WatchMachReceivePort(port, controller,
                                                     delegate);
}
#endif

#endif  // !defined(OS_NACL_SFI)

#if defined(OS_FUCHSIA)
// Additional watch API for native platform resources.
bool CurrentIOThread::WatchZxHandle(
    zx_handle_t handle,
    bool persistent,
    zx_signals_t signals,
    MessagePumpForIO::ZxHandleWatchController* controller,
    MessagePumpForIO::ZxHandleWatcher* delegate) {
  DCHECK(current_->IsBoundToCurrentThread());
  return GetMessagePumpForIO()->WatchZxHandle(handle, persistent, signals,
                                              controller, delegate);
}
#endif

}  // namespace base
