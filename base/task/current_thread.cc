// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/current_thread.h"

#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

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

void CurrentThread::EnableMessagePumpTimeKeeperMetrics(
    const char* thread_name,
    bool wall_time_based_metrics_enabled_for_testing) {
  return current_->EnableMessagePumpTimeKeeperMetrics(
      thread_name, wall_time_based_metrics_enabled_for_testing);
}

void CurrentThread::AddTaskObserver(TaskObserver* task_observer) {
  DCHECK(current_->IsBoundToCurrentThread());
  current_->AddTaskObserver(task_observer);
}

void CurrentThread::RemoveTaskObserver(TaskObserver* task_observer) {
  DCHECK(current_->IsBoundToCurrentThread());
  current_->RemoveTaskObserver(task_observer);
}

void CurrentThread::SetAddQueueTimeToTasks(bool enable) {
  DCHECK(current_->IsBoundToCurrentThread());
  current_->SetAddQueueTimeToTasks(enable);
}

CallbackListSubscription CurrentThread::RegisterOnNextIdleCallback(
    RegisterOnNextIdleCallbackPasskey,
    OnceClosure on_next_idle_callback) {
  return current_->RegisterOnNextIdleCallback(std::move(on_next_idle_callback));
}

CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop::
    ScopedAllowApplicationTasksInNativeNestedLoop()
    : sequence_manager_(GetCurrentSequenceManagerImpl()),
      previous_state_(
          sequence_manager_->IsTaskExecutionAllowedInNativeNestedLoop()) {
  TRACE_EVENT_BEGIN0("base", "ScopedNestableTaskAllower");
  sequence_manager_->SetTaskExecutionAllowedInNativeNestedLoop(true);
}

CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop::
    ~ScopedAllowApplicationTasksInNativeNestedLoop() {
  sequence_manager_->SetTaskExecutionAllowedInNativeNestedLoop(previous_state_);
  TRACE_EVENT_END0("base", "ScopedNestableTaskAllower");
}

bool CurrentThread::ApplicationTasksAllowedInNativeNestedLoop() const {
  return current_->IsTaskExecutionAllowedInNativeNestedLoop();
}

#if !BUILDFLAG(IS_NACL)

//------------------------------------------------------------------------------
// CurrentUIThread

// static
CurrentUIThread CurrentUIThread::Get() {
  auto* sequence_manager = GetCurrentSequenceManagerImpl();
  DCHECK(sequence_manager);
#if BUILDFLAG(IS_ANDROID)
  DCHECK(sequence_manager->IsType(MessagePumpType::UI) ||
         sequence_manager->IsType(MessagePumpType::JAVA));
#else   // BUILDFLAG(IS_ANDROID)
  DCHECK(sequence_manager->IsType(MessagePumpType::UI));
#endif  // BUILDFLAG(IS_ANDROID)
  return CurrentUIThread(sequence_manager);
}

// static
bool CurrentUIThread::IsSet() {
  sequence_manager::internal::SequenceManagerImpl* sequence_manager =
      GetCurrentSequenceManagerImpl();
  return sequence_manager &&
#if BUILDFLAG(IS_ANDROID)
         (sequence_manager->IsType(MessagePumpType::UI) ||
          sequence_manager->IsType(MessagePumpType::JAVA));
#else   // BUILDFLAG(IS_ANDROID)
         sequence_manager->IsType(MessagePumpType::UI);
#endif  // BUILDFLAG(IS_ANDROID)
}

MessagePumpForUI* CurrentUIThread::GetMessagePumpForUI() const {
  return static_cast<MessagePumpForUI*>(current_->GetMessagePump());
}

#if BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_WIN)
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

#if BUILDFLAG(IS_IOS)
void CurrentUIThread::Attach() {
  current_->AttachToMessagePump();
}
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
void CurrentUIThread::Abort() {
  GetMessagePumpForUI()->Abort();
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
void CurrentUIThread::AddMessagePumpObserver(
    MessagePumpForUI::Observer* observer) {
  GetMessagePumpForUI()->AddObserver(observer);
}

void CurrentUIThread::RemoveMessagePumpObserver(
    MessagePumpForUI::Observer* observer) {
  GetMessagePumpForUI()->RemoveObserver(observer);
}
#endif  // BUILDFLAG(IS_WIN)

#endif  // !BUILDFLAG(IS_NACL)

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

#if !BUILDFLAG(IS_NACL)

#if BUILDFLAG(IS_WIN)
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

#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
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
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_IOS) && !BUILDFLAG(CRONET_BUILD))
bool CurrentIOThread::WatchMachReceivePort(
    mach_port_t port,
    MessagePumpForIO::MachPortWatchController* controller,
    MessagePumpForIO::MachPortWatcher* delegate) {
  DCHECK(current_->IsBoundToCurrentThread());
  return GetMessagePumpForIO()->WatchMachReceivePort(port, controller,
                                                     delegate);
}
#endif

#endif  // !BUILDFLAG(IS_NACL)

#if BUILDFLAG(IS_FUCHSIA)
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
