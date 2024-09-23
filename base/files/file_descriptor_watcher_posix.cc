// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_descriptor_watcher_posix.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"

namespace base {

namespace {

// Per-thread FileDescriptorWatcher registration.
constinit thread_local FileDescriptorWatcher* fd_watcher = nullptr;

}  // namespace

class FileDescriptorWatcher::Controller::Watcher
    : public MessagePumpForIO::FdWatcher,
      public CurrentThread::DestructionObserver {
 public:
  Watcher(WeakPtr<Controller> controller,
          base::WaitableEvent& on_destroyed,
          MessagePumpForIO::Mode mode,
          int fd);
  Watcher(const Watcher&) = delete;
  Watcher& operator=(const Watcher&) = delete;
  ~Watcher() override;

  void StartWatching();

 private:
  friend class FileDescriptorWatcher;

  // MessagePumpForIO::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  // The MessagePumpForIO's watch handle (stops the watch when destroyed).
  MessagePumpForIO::FdWatchController fd_watch_controller_;

  // Runs tasks on the sequence on which this was instantiated (i.e. the
  // sequence on which the callback must run).
  const scoped_refptr<SequencedTaskRunner> callback_task_runner_ =
      SequencedTaskRunner::GetCurrentDefault();

  // The Controller that created this Watcher. This WeakPtr is bound to the
  // |controller_| thread and can only be used by this Watcher to post back to
  // |callback_task_runner_|.
  WeakPtr<Controller> controller_;

  // WaitableEvent to signal to ensure that the Watcher is always destroyed
  // before the Controller.
  const raw_ref<base::WaitableEvent, AcrossTasksDanglingUntriaged>
      on_destroyed_;

  // Whether this Watcher is notified when |fd_| becomes readable or writable
  // without blocking.
  const MessagePumpForIO::Mode mode_;

  // The watched file descriptor.
  const int fd_;

  // Except for the constructor, every method of this class must run on the same
  // MessagePumpForIO thread.
  ThreadChecker thread_checker_;

  // Whether this Watcher was registered as a DestructionObserver on the
  // MessagePumpForIO thread.
  bool registered_as_destruction_observer_ = false;
};

FileDescriptorWatcher::Controller::Watcher::Watcher(
    WeakPtr<Controller> controller,
    base::WaitableEvent& on_destroyed,
    MessagePumpForIO::Mode mode,
    int fd)
    : fd_watch_controller_(FROM_HERE),
      controller_(controller),
      on_destroyed_(on_destroyed),
      mode_(mode),
      fd_(fd) {
  DCHECK(callback_task_runner_);
  thread_checker_.DetachFromThread();
}

FileDescriptorWatcher::Controller::Watcher::~Watcher() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CurrentIOThread::Get()->RemoveDestructionObserver(this);

  // Stop watching the descriptor before signalling |on_destroyed_|.
  CHECK(fd_watch_controller_.StopWatchingFileDescriptor());
  on_destroyed_->Signal();
}

void FileDescriptorWatcher::Controller::Watcher::StartWatching() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(CurrentIOThread::IsSet());

  const bool watch_success = CurrentIOThread::Get()->WatchFileDescriptor(
      fd_, false, mode_, &fd_watch_controller_, this);
  DCHECK(watch_success) << "Failed to watch fd=" << fd_;

  if (!registered_as_destruction_observer_) {
    CurrentIOThread::Get()->AddDestructionObserver(this);
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

  if (callback_task_runner_->RunsTasksInCurrentSequence()) {
    // |controller_| can be accessed directly when Watcher runs on the same
    // thread.
    Watcher* watcher = controller_->watcher_;
    controller_->watcher_ = nullptr;
    delete watcher;
  } else {
    // If the Watcher and the Controller live on different threads, delete
    // |this| synchronously. Pending tasks bound to an unretained Watcher* will
    // not run since this loop is dead. The associated Controller will not know
    // whether the Watcher has been destroyed but it never uses it directly and
    // will ultimately send it to this thread for deletion (and that also  won't
    // run since the loop being dead).
    delete this;
  }
}

FileDescriptorWatcher::Controller::Controller(MessagePumpForIO::Mode mode,
                                              int fd,
                                              const RepeatingClosure& callback)
    : callback_(callback),
      io_thread_task_runner_(fd_watcher->io_thread_task_runner()) {
  DCHECK(!callback_.is_null());
  DCHECK(io_thread_task_runner_);
  watcher_ =
      new Watcher(weak_factory_.GetWeakPtr(), on_watcher_destroyed_, mode, fd);
  StartWatching();
}

FileDescriptorWatcher::Controller::~Controller() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (io_thread_task_runner_->BelongsToCurrentThread()) {
    // If the MessagePumpForIO and the Controller live on the same thread.
    if (watcher_)
      delete watcher_;
  } else {
    // Synchronously wait until |watcher_| is deleted on the MessagePumpForIO
    // thread. This ensures that the file descriptor is never accessed after
    // this destructor returns.
    //
    // We considered associating "generations" to file descriptors to avoid the
    // synchronous wait. For example, if the IO thread gets a "cancel" for fd=6,
    // generation=1 after getting a "start watching" for fd=6, generation=2, it
    // can ignore the "Cancel". However, "generations" didn't solve this race:
    //
    // T1 (client) Start watching fd = 6 with WatchReadable()
    //             Stop watching fd = 6
    //             Close fd = 6
    //             Open a new file, fd = 6 gets reused.
    // T2 (io)     Watcher::StartWatching()
    //               Incorrectly starts watching fd = 6 which now refers to a
    //               different file than when WatchReadable() was called.
    auto delete_task = BindOnce(
        [](Watcher* watcher) {
          // Since |watcher| is a raw pointer, it isn't deleted if this callback
          // is deleted before it gets to run.
          delete watcher;
        },
        UnsafeDanglingUntriaged(watcher_));
    io_thread_task_runner_->PostTask(FROM_HERE, std::move(delete_task));
    ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow;
    on_watcher_destroyed_.Wait();
  }

  // Since WeakPtrs are invalidated by the destructor, any pending RunCallback()
  // won't be invoked after this returns.
}

void FileDescriptorWatcher::Controller::StartWatching() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (io_thread_task_runner_->BelongsToCurrentThread()) {
    // If the MessagePumpForIO and the Controller live on the same thread.
    watcher_->StartWatching();
  } else {
    // It is safe to use Unretained() below because |watcher_| can only be
    // deleted by a delete task posted to |io_thread_task_runner_| by this
    // Controller's destructor. Since this delete task hasn't been posted yet,
    // it can't run before the task posted below.
    io_thread_task_runner_->PostTask(
        FROM_HERE, BindOnce(&Watcher::StartWatching, Unretained(watcher_)));
  }
}

void FileDescriptorWatcher::Controller::RunCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  WeakPtr<Controller> weak_this = weak_factory_.GetWeakPtr();

  // Run a copy of the callback in case this Controller is deleted by the
  // callback. This would cause the callback itself to be deleted while it is
  // being run.
  RepeatingClosure callback_copy = callback_;
  callback_copy.Run();

  // If |this| wasn't deleted, re-enable the watch.
  if (weak_this)
    StartWatching();
}

FileDescriptorWatcher::FileDescriptorWatcher(
    scoped_refptr<SingleThreadTaskRunner> io_thread_task_runner)
    : resetter_(&fd_watcher, this, nullptr),
      io_thread_task_runner_(std::move(io_thread_task_runner)) {}

FileDescriptorWatcher::~FileDescriptorWatcher() = default;

std::unique_ptr<FileDescriptorWatcher::Controller>
FileDescriptorWatcher::WatchReadable(int fd, const RepeatingClosure& callback) {
  return WrapUnique(new Controller(MessagePumpForIO::WATCH_READ, fd, callback));
}

std::unique_ptr<FileDescriptorWatcher::Controller>
FileDescriptorWatcher::WatchWritable(int fd, const RepeatingClosure& callback) {
  return WrapUnique(
      new Controller(MessagePumpForIO::WATCH_WRITE, fd, callback));
}

#if DCHECK_IS_ON()
void FileDescriptorWatcher::AssertAllowed() {
  DCHECK(fd_watcher);
}
#endif

}  // namespace base
