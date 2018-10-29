// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop_current.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/no_destructor.h"
#include "base/threading/thread_local.h"

namespace base {

namespace {

base::ThreadLocalPointer<MessageLoop>* GetTLSMessageLoop() {
  static NoDestructor<ThreadLocalPointer<MessageLoop>> lazy_tls_ptr;
  return lazy_tls_ptr.get();
}

}  // namespace

// static
MessageLoop* MessageLoop::GetCurrentDeprecated() {
  return GetTLSMessageLoop()->Get();
}

//------------------------------------------------------------------------------
// MessageLoopCurrent

// static
MessageLoopCurrent MessageLoopCurrent::Get() {
  return MessageLoopCurrent(GetTLSMessageLoop()->Get());
}

// static
bool MessageLoopCurrent::IsSet() {
  return !!GetTLSMessageLoop()->Get();
}

void MessageLoopCurrent::AddDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  current_->destruction_observers_.AddObserver(destruction_observer);
}

void MessageLoopCurrent::RemoveDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  current_->destruction_observers_.RemoveObserver(destruction_observer);
}

std::string MessageLoopCurrent::GetThreadName() const {
  return current_->GetThreadName();
}

const scoped_refptr<SingleThreadTaskRunner>& MessageLoopCurrent::task_runner()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return current_->task_runner();
}

void MessageLoopCurrent::SetTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  current_->SetTaskRunner(std::move(task_runner));
}

bool MessageLoopCurrent::IsBoundToCurrentThread() const {
  return current_ == GetTLSMessageLoop()->Get();
}

bool MessageLoopCurrent::IsIdleForTesting() {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return current_->IsIdleForTesting();
}

void MessageLoopCurrent::AddTaskObserver(TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  current_->AddTaskObserver(task_observer);
}

void MessageLoopCurrent::RemoveTaskObserver(TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  current_->RemoveTaskObserver(task_observer);
}

void MessageLoopCurrent::SetAddQueueTimeToTasks(bool enable) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  current_->SetAddQueueTimeToTasks(enable);
}

void MessageLoopCurrent::SetNestableTasksAllowed(bool allowed) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  if (allowed) {
    // Kick the native pump just in case we enter a OS-driven nested message
    // loop that does not go through RunLoop::Run().
    current_->pump_->ScheduleWork();
  }
  current_->task_execution_allowed_ = allowed;
}

bool MessageLoopCurrent::NestableTasksAllowed() const {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return current_->task_execution_allowed_;
}

MessageLoopCurrent::ScopedNestableTaskAllower::ScopedNestableTaskAllower()
    : loop_(GetTLSMessageLoop()->Get()),
      old_state_(loop_->NestableTasksAllowed()) {
  loop_->SetNestableTasksAllowed(true);
}

MessageLoopCurrent::ScopedNestableTaskAllower::~ScopedNestableTaskAllower() {
  loop_->SetNestableTasksAllowed(old_state_);
}

// static
void MessageLoopCurrent::BindToCurrentThreadInternal(MessageLoop* current) {
  DCHECK(!GetTLSMessageLoop()->Get())
      << "Can't register a second MessageLoop on the same thread.";
  GetTLSMessageLoop()->Set(current);
}

// static
void MessageLoopCurrent::UnbindFromCurrentThreadInternal(MessageLoop* current) {
  DCHECK_EQ(current, GetTLSMessageLoop()->Get());
  GetTLSMessageLoop()->Set(nullptr);
}

#if !defined(OS_NACL)

//------------------------------------------------------------------------------
// MessageLoopCurrentForUI

// static
MessageLoopCurrentForUI MessageLoopCurrentForUI::Get() {
  MessageLoop* loop = GetTLSMessageLoop()->Get();
  DCHECK(loop);
#if defined(OS_ANDROID)
  DCHECK(loop->IsType(MessageLoop::TYPE_UI) ||
         loop->IsType(MessageLoop::TYPE_JAVA));
#else   // defined(OS_ANDROID)
  DCHECK(loop->IsType(MessageLoop::TYPE_UI));
#endif  // defined(OS_ANDROID)
  auto* loop_for_ui = static_cast<MessageLoopForUI*>(loop);
  return MessageLoopCurrentForUI(
      loop_for_ui, static_cast<MessagePumpForUI*>(loop_for_ui->pump_.get()));
}

// static
bool MessageLoopCurrentForUI::IsSet() {
  MessageLoop* loop = GetTLSMessageLoop()->Get();
  return loop &&
#if defined(OS_ANDROID)
         (loop->IsType(MessageLoop::TYPE_UI) ||
          loop->IsType(MessageLoop::TYPE_JAVA));
#else   // defined(OS_ANDROID)
         loop->IsType(MessageLoop::TYPE_UI);
#endif  // defined(OS_ANDROID)
}

#if defined(USE_OZONE) && !defined(OS_FUCHSIA) && !defined(OS_WIN)
bool MessageLoopCurrentForUI::WatchFileDescriptor(
    int fd,
    bool persistent,
    MessagePumpForUI::Mode mode,
    MessagePumpForUI::FdWatchController* controller,
    MessagePumpForUI::FdWatcher* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return pump_->WatchFileDescriptor(fd, persistent, mode, controller, delegate);
}
#endif

#if defined(OS_IOS)
void MessageLoopCurrentForUI::Attach() {
  static_cast<MessageLoopForUI*>(current_)->Attach();
}
#endif  // defined(OS_IOS)

#if defined(OS_ANDROID)
void MessageLoopCurrentForUI::Abort() {
  static_cast<MessageLoopForUI*>(current_)->Abort();
}
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
void MessageLoopCurrentForUI::AddMessagePumpObserver(
    MessagePumpForUI::Observer* observer) {
  pump_->AddObserver(observer);
}

void MessageLoopCurrentForUI::RemoveMessagePumpObserver(
    MessagePumpForUI::Observer* observer) {
  pump_->RemoveObserver(observer);
}
#endif  // defined(OS_WIN)

#endif  // !defined(OS_NACL)

//------------------------------------------------------------------------------
// MessageLoopCurrentForIO

// static
MessageLoopCurrentForIO MessageLoopCurrentForIO::Get() {
  MessageLoop* loop = GetTLSMessageLoop()->Get();
  DCHECK(loop);
  DCHECK_EQ(MessageLoop::TYPE_IO, loop->type());
  auto* loop_for_io = static_cast<MessageLoopForIO*>(loop);
  return MessageLoopCurrentForIO(
      loop_for_io, static_cast<MessagePumpForIO*>(loop_for_io->pump_.get()));
}

// static
bool MessageLoopCurrentForIO::IsSet() {
  MessageLoop* loop = GetTLSMessageLoop()->Get();
  return loop && loop->IsType(MessageLoop::TYPE_IO);
}

#if !defined(OS_NACL_SFI)

#if defined(OS_WIN)
HRESULT MessageLoopCurrentForIO::RegisterIOHandler(
    HANDLE file,
    MessagePumpForIO::IOHandler* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return pump_->RegisterIOHandler(file, handler);
}

bool MessageLoopCurrentForIO::RegisterJobObject(
    HANDLE job,
    MessagePumpForIO::IOHandler* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return pump_->RegisterJobObject(job, handler);
}

bool MessageLoopCurrentForIO::WaitForIOCompletion(
    DWORD timeout,
    MessagePumpForIO::IOHandler* filter) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return pump_->WaitForIOCompletion(timeout, filter);
}
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
bool MessageLoopCurrentForIO::WatchFileDescriptor(
    int fd,
    bool persistent,
    MessagePumpForIO::Mode mode,
    MessagePumpForIO::FdWatchController* controller,
    MessagePumpForIO::FdWatcher* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return pump_->WatchFileDescriptor(fd, persistent, mode, controller, delegate);
}
#endif  // defined(OS_WIN)

#endif  // !defined(OS_NACL_SFI)

#if defined(OS_FUCHSIA)
// Additional watch API for native platform resources.
bool MessageLoopCurrentForIO::WatchZxHandle(
    zx_handle_t handle,
    bool persistent,
    zx_signals_t signals,
    MessagePumpForIO::ZxHandleWatchController* controller,
    MessagePumpForIO::ZxHandleWatcher* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(current_->bound_thread_checker_);
  return pump_->WatchZxHandle(handle, persistent, signals, controller,
                              delegate);
}
#endif

}  // namespace base
