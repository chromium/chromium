// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_io_ios_libdispatch.h"

namespace base {

MessagePumpIOSForIOLibdispatch::FdWatchController::FdWatchController(
    const Location& location)
    : FdWatchControllerInterface(location) {}

MessagePumpIOSForIOLibdispatch::FdWatchController::~FdWatchController() {
  StopWatchingFileDescriptor();
}

bool MessagePumpIOSForIOLibdispatch::FdWatchController::
    StopWatchingFileDescriptor() {
  watcher_ = nullptr;
  fd_ = -1;
  dispatch_source_read_.reset();
  dispatch_source_write_.reset();
  return true;
}

void MessagePumpIOSForIOLibdispatch::FdWatchController::Init(
    const scoped_refptr<base::SequencedTaskRunner>& io_thread_task_runner,
    dispatch_queue_t queue,
    int fd,
    bool persistent,
    int mode,
    FdWatcher* watcher) {
  DCHECK(io_thread_task_runner->RunsTasksInCurrentSequence());
  DCHECK(watcher);
  DCHECK(!watcher_);
  is_persistent_ = persistent;
  io_thread_task_runner_ = io_thread_task_runner;
  fd_ = fd;
  watcher_ = watcher;
  base::WeakPtr<MessagePumpIOSForIOLibdispatch::FdWatchController> weak_this =
      weak_factory_.GetWeakPtr();
  DCHECK(mode == WATCH_READ || mode == WATCH_WRITE || mode == WATCH_READ_WRITE);

  if (mode == WATCH_READ || mode == WATCH_READ_WRITE) {
    dispatch_source_read_ = std::make_unique<
        apple::DispatchSource>(queue, fd, DISPATCH_SOURCE_TYPE_READ, ^{
      if (fd_ == -1) {
        return;
      }
      dispatch_source_read_->Suspend();
      io_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &MessagePumpIOSForIOLibdispatch::FdWatchController::HandleRead,
              weak_this));
    });
    dispatch_source_read_->Resume();
  }
  if (mode == WATCH_WRITE || mode == WATCH_READ_WRITE) {
    dispatch_source_write_ = std::make_unique<
        apple::DispatchSource>(queue, fd, DISPATCH_SOURCE_TYPE_WRITE, ^{
      if (fd_ == -1) {
        return;
      }
      dispatch_source_write_->Suspend();
      io_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &MessagePumpIOSForIOLibdispatch::FdWatchController::HandleWrite,
              weak_this));
    });
    dispatch_source_write_->Resume();
  }
}

void MessagePumpIOSForIOLibdispatch::FdWatchController::HandleRead() {
  DCHECK(io_thread_task_runner_->RunsTasksInCurrentSequence());
  if (watcher_) {
    base::WeakPtr<MessagePumpIOSForIOLibdispatch::FdWatchController> weak_this =
        weak_factory_.GetWeakPtr();
    watcher_->OnFileCanReadWithoutBlocking(fd_);
    if (!weak_this) {
      return;
    }
  }
  if (!is_persistent_) {
    StopWatchingFileDescriptor();
  }
  if (dispatch_source_read_) {
    dispatch_source_read_->Resume();
  }
}

void MessagePumpIOSForIOLibdispatch::FdWatchController::HandleWrite() {
  DCHECK(io_thread_task_runner_->RunsTasksInCurrentSequence());
  if (watcher_) {
    base::WeakPtr<MessagePumpIOSForIOLibdispatch::FdWatchController> weak_this =
        weak_factory_.GetWeakPtr();
    watcher_->OnFileCanWriteWithoutBlocking(fd_);
    if (!weak_this) {
      return;
    }
  }
  if (!is_persistent_) {
    StopWatchingFileDescriptor();
  }
  if (dispatch_source_write_) {
    dispatch_source_write_->Resume();
  }
}

MessagePumpIOSForIOLibdispatch::MachPortWatchController::
    MachPortWatchController(const Location& location) {}

MessagePumpIOSForIOLibdispatch::MachPortWatchController::
    ~MachPortWatchController() {
  StopWatchingMachPort();
}

bool MessagePumpIOSForIOLibdispatch::MachPortWatchController::
    StopWatchingMachPort() {
  port_ = MACH_PORT_NULL;
  watcher_ = nullptr;
  dispatch_source_.reset();
  return true;
}

void MessagePumpIOSForIOLibdispatch::MachPortWatchController::Init(
    const scoped_refptr<base::SequencedTaskRunner>& io_thread_task_runner,
    dispatch_queue_t queue,
    mach_port_t port,
    MachPortWatcher* watcher) {
  DCHECK(io_thread_task_runner->RunsTasksInCurrentSequence());
  DCHECK(watcher);
  DCHECK(!watcher_);
  watcher_ = watcher;
  port_ = port;
  io_thread_task_runner_ = io_thread_task_runner;
  base::WeakPtr<MessagePumpIOSForIOLibdispatch::MachPortWatchController>
      weak_this = weak_factory_.GetWeakPtr();
  dispatch_source_ = std::make_unique<apple::DispatchSource>(queue, port, ^{
    if (port_ == MACH_PORT_NULL) {
      return;
    }
    dispatch_source_->Suspend();
    io_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MessagePumpIOSForIOLibdispatch::
                                      MachPortWatchController::HandleReceive,
                                  weak_this));
  });
  dispatch_source_->Resume();
}

void MessagePumpIOSForIOLibdispatch::MachPortWatchController::HandleReceive() {
  DCHECK(io_thread_task_runner_->RunsTasksInCurrentSequence());
  base::WeakPtr<MessagePumpIOSForIOLibdispatch::MachPortWatchController>
      weak_this = weak_factory_.GetWeakPtr();
  watcher_->OnMachMessageReceived(port_);
  if (!weak_this) {
    return;
  }
  if (dispatch_source_) {
    dispatch_source_->Resume();
  }
}

MessagePumpIOSForIOLibdispatch::MessagePumpIOSForIOLibdispatch()
    : queue_(dispatch_queue_create("org.chromium.io_thread.libdispatch_bridge",
                                   DISPATCH_QUEUE_SERIAL)) {}

MessagePumpIOSForIOLibdispatch::~MessagePumpIOSForIOLibdispatch() {
  dispatch_release(queue_);
}

bool MessagePumpIOSForIOLibdispatch::WatchFileDescriptor(
    int fd,
    bool persistent,
    int mode,
    FdWatchController* controller,
    FdWatcher* watcher) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GE(fd, 0);
  DCHECK(controller);
  DCHECK(watcher);
  DCHECK(mode == WATCH_READ || mode == WATCH_WRITE || mode == WATCH_READ_WRITE);

  // WatchFileDescriptor is allowed to be called again if the file handle
  // matches the one that is currently bound.
  if (controller->fd() != -1 && controller->fd() != fd) {
    return false;
  }
  controller->StopWatchingFileDescriptor();

  controller->Init(SequencedTaskRunner::GetCurrentDefault(), queue_, fd,
                   persistent, mode, watcher);
  return true;
}

bool MessagePumpIOSForIOLibdispatch::WatchMachReceivePort(
    mach_port_t port,
    MachPortWatchController* controller,
    MachPortWatcher* watcher) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(port, static_cast<mach_port_t>(MACH_PORT_NULL));
  DCHECK(controller);
  DCHECK(watcher);
  controller->Init(SequencedTaskRunner::GetCurrentDefault(), queue_, port,
                   watcher);
  return true;
}

}  // namespace base
