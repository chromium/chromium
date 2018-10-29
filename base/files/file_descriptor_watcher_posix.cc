// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_descriptor_watcher_posix.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"

namespace base {

namespace {

// MessageLoopForIO used to watch file descriptors for which callbacks are
// registered from a given thread.
LazyInstance<ThreadLocalPointer<FileDescriptorWatcher>>::Leaky tls_fd_watcher =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

FileDescriptorWatcher::Controller::~Controller() {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  // Delete |watcher_| on the IO thread task runner.
  //
  // If the MessageLoopForIO is deleted before Watcher::StartWatching() runs,
  // |watcher_| is leaked. If the MessageLoopForIO is deleted after
  // Watcher::StartWatching() runs but before the DeleteSoon task runs,
  // |watcher_| is deleted from Watcher::WillDestroyCurrentMessageLoop().
  io_thread_task_runner_->DeleteSoon(FROM_HERE, watcher_.release());

  // Since WeakPtrs are invalidated by the destructor, RunCallback() won't be
  // invoked after this returns.
}

class FileDescriptorWatcher::Controller::Watcher
    : public MessagePumpForIO::FdWatcher,
      public MessageLoopCurrent::DestructionObserver {
 public:
  Watcher(WeakPtr<Controller> controller, MessagePumpForIO::Mode mode, int fd);
  ~Watcher() override;

  void StartWatching();

 private:
  friend class FileDescriptorWatcher;

  // MessagePumpForIO::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // MessageLoopCurrent::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  // The MessageLoopForIO's watch handle (stops the watch when destroyed).
  MessagePumpForIO::FdWatchController fd_watch_controller_;

  // Runs tasks on the sequence on which this was instantiated (i.e. the
  // sequence on which the callback must run).
  const scoped_refptr<SequencedTaskRunner> callback_task_runner_ =
      SequencedTaskRunnerHandle::Get();

  // The Controller that created this Watcher.
  WeakPtr<Controller> controller_;

  // Whether this Watcher is notified when |fd_| becomes readable or writable
  // without blocking.
  const MessagePumpForIO::Mode mode_;

  // The watched file descriptor.
  const int fd_;

  // Except for the constructor, every method of this class must run on the same
  // MessageLoopForIO thread.
  ThreadChecker thread_checker_;

  // Whether this Watcher was registered as a DestructionObserver on the
  // MessageLoopForIO thread.
  bool registered_as_destruction_observer_ = false;

  DISALLOW_COPY_AND_ASSIGN(Watcher);
};

FileDescriptorWatcher::Controller::Watcher::Watcher(
    WeakPtr<Controller> controller,
    MessagePumpForIO::Mode mode,
    int fd)
    : fd_watch_controller_(FROM_HERE),
      controller_(controller),
      mode_(mode),
      fd_(fd) {
  DCHECK(callback_task_runner_);
  thread_checker_.DetachFromThread();
}

FileDescriptorWatcher::Controller::Watcher::~Watcher() {
  DCHECK(thread_checker_.CalledOnValidThread());
  MessageLoopCurrentForIO::Get()->RemoveDestructionObserver(this);
}

void FileDescriptorWatcher::Controller::Watcher::StartWatching() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(MessageLoopCurrentForIO::IsSet());

  if (!MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
          fd_, false, mode_, &fd_watch_controller_, this)) {
    // TODO(wez): Ideally we would [D]CHECK here, or propagate the failure back
    // to the caller, but there is no guarantee that they haven't already
    // closed |fd_| on another thread, so the best we can do is Debug-log.
    DLOG(ERROR) << "Failed to watch fd=" << fd_;
  }

  if (!registered_as_destruction_observer_) {
    MessageLoopCurrentForIO::Get()->AddDestructionObserver(this);
    registered_as_destruction_observer_ = true;
  }
}

void FileDescriptorWatcher::Controller::Watcher::OnFileCanReadWithoutBlocking(
    int fd) {
  DCHECK_EQ(fd_, fd);
  DCHECK_EQ(MessagePumpForIO::WATCH_READ, mode_);
  DCHECK(thread_checker_.CalledOnValidThread());

  // Run the callback on the sequence on which the watch was initiated.
  callback_task_runner_->PostTask(
      FROM_HERE, BindOnce(&Controller::RunCallback, controller_));
}

void FileDescriptorWatcher::Controller::Watcher::OnFileCanWriteWithoutBlocking(
    int fd) {
  DCHECK_EQ(fd_, fd);
  DCHECK_EQ(MessagePumpForIO::WATCH_WRITE, mode_);
  DCHECK(thread_checker_.CalledOnValidThread());

  // Run the callback on the sequence on which the watch was initiated.
  callback_task_runner_->PostTask(
      FROM_HERE, BindOnce(&Controller::RunCallback, controller_));
}

void FileDescriptorWatcher::Controller::Watcher::
    WillDestroyCurrentMessageLoop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // A Watcher is owned by a Controller. When the Controller is deleted, it
  // transfers ownership of the Watcher to a delete task posted to the
  // MessageLoopForIO. If the MessageLoopForIO is deleted before the delete task
  // runs, the following line takes care of deleting the Watcher.
  delete this;
}

FileDescriptorWatcher::Controller::Controller(MessagePumpForIO::Mode mode,
                                              int fd,
                                              const Closure& callback)
    : callback_(callback),
      io_thread_task_runner_(
          tls_fd_watcher.Get().Get()->io_thread_task_runner()),
      weak_factory_(this) {
  DCHECK(!callback_.is_null());
  DCHECK(io_thread_task_runner_);
  watcher_ = std::make_unique<Watcher>(weak_factory_.GetWeakPtr(), mode, fd);
  StartWatching();
}

void FileDescriptorWatcher::Controller::StartWatching() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  // It is safe to use Unretained() below because |watcher_| can only be deleted
  // by a delete task posted to |io_thread_task_runner_| by this
  // Controller's destructor. Since this delete task hasn't been posted yet, it
  // can't run before the task posted below.
  io_thread_task_runner_->PostTask(
      FROM_HERE, BindOnce(&Watcher::StartWatching, Unretained(watcher_.get())));
}

void FileDescriptorWatcher::Controller::RunCallback() {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  WeakPtr<Controller> weak_this = weak_factory_.GetWeakPtr();

  callback_.Run();

  // If |this| wasn't deleted, re-enable the watch.
  if (weak_this)
    StartWatching();
}

FileDescriptorWatcher::FileDescriptorWatcher(
    scoped_refptr<SingleThreadTaskRunner> io_thread_task_runner)
    : io_thread_task_runner_(std::move(io_thread_task_runner)) {
  DCHECK(!tls_fd_watcher.Get().Get());
  tls_fd_watcher.Get().Set(this);
}

FileDescriptorWatcher::~FileDescriptorWatcher() {
  tls_fd_watcher.Get().Set(nullptr);
}

std::unique_ptr<FileDescriptorWatcher::Controller>
FileDescriptorWatcher::WatchReadable(int fd, const Closure& callback) {
  return WrapUnique(new Controller(MessagePumpForIO::WATCH_READ, fd, callback));
}

std::unique_ptr<FileDescriptorWatcher::Controller>
FileDescriptorWatcher::WatchWritable(int fd, const Closure& callback) {
  return WrapUnique(
      new Controller(MessagePumpForIO::WATCH_WRITE, fd, callback));
}

}  // namespace base
