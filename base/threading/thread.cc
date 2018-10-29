// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_NACL)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace base {

namespace {

// We use this thread-local variable to record whether or not a thread exited
// because its Stop method was called.  This allows us to catch cases where
// MessageLoop::QuitWhenIdle() is called directly, which is unexpected when
// using a Thread to setup and run a MessageLoop.
base::LazyInstance<base::ThreadLocalBoolean>::Leaky lazy_tls_bool =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

Thread::Options::Options() = default;

Thread::Options::Options(MessageLoop::Type type, size_t size)
    : message_loop_type(type), stack_size(size) {}

Thread::Options::Options(const Options& other) = default;

Thread::Options::~Options() = default;

Thread::Thread(const std::string& name)
    : id_event_(WaitableEvent::ResetPolicy::MANUAL,
                WaitableEvent::InitialState::NOT_SIGNALED),
      name_(name),
      start_event_(WaitableEvent::ResetPolicy::MANUAL,
                   WaitableEvent::InitialState::NOT_SIGNALED) {
  // Only bind the sequence on Start(): the state is constant between
  // construction and Start() and it's thus valid for Start() to be called on
  // another sequence as long as every other operation is then performed on that
  // sequence.
  owning_sequence_checker_.DetachFromSequence();
}

Thread::~Thread() {
  Stop();
}

bool Thread::Start() {
  DCHECK(owning_sequence_checker_.CalledOnValidSequence());

  Options options;
#if defined(OS_WIN)
  if (com_status_ == STA)
    options.message_loop_type = MessageLoop::TYPE_UI;
#endif
  return StartWithOptions(options);
}

bool Thread::StartWithOptions(const Options& options) {
  DCHECK(owning_sequence_checker_.CalledOnValidSequence());
  DCHECK(!message_loop_);
  DCHECK(!IsRunning());
  DCHECK(!stopping_) << "Starting a non-joinable thread a second time? That's "
                     << "not allowed!";
#if defined(OS_WIN)
  DCHECK((com_status_ != STA) ||
      (options.message_loop_type == MessageLoop::TYPE_UI));
#endif

  // Reset |id_| here to support restarting the thread.
  id_event_.Reset();
  id_ = kInvalidThreadId;

  SetThreadWasQuitProperly(false);

  MessageLoop::Type type = options.message_loop_type;
  if (!options.message_pump_factory.is_null())
    type = MessageLoop::TYPE_CUSTOM;

  message_loop_timer_slack_ = options.timer_slack;
  std::unique_ptr<MessageLoop> message_loop_owned =
      MessageLoop::CreateUnbound(type, options.message_pump_factory);
  message_loop_ = message_loop_owned.get();
  start_event_.Reset();

  if (options.on_sequence_manager_created) {
    sequence_manager_ =
        sequence_manager::CreateUnboundSequenceManager(message_loop_);
    options.on_sequence_manager_created.Run(sequence_manager_.get());
  }

  // Hold |thread_lock_| while starting the new thread to synchronize with
  // Stop() while it's not guaranteed to be sequenced (until crbug/629139 is
  // fixed).
  {
    AutoLock lock(thread_lock_);
    bool success =
        options.joinable
            ? PlatformThread::CreateWithPriority(options.stack_size, this,
                                                 &thread_, options.priority)
            : PlatformThread::CreateNonJoinableWithPriority(
                  options.stack_size, this, options.priority);
    if (!success) {
      DLOG(ERROR) << "failed to create thread";
      message_loop_ = nullptr;
      return false;
    }
  }

  joinable_ = options.joinable;

  // The ownership of |message_loop_| is managed by the newly created thread
  // within the ThreadMain.
  ignore_result(message_loop_owned.release());

  DCHECK(message_loop_);
  return true;
}

bool Thread::StartAndWaitForTesting() {
  DCHECK(owning_sequence_checker_.CalledOnValidSequence());
  bool result = Start();
  if (!result)
    return false;
  WaitUntilThreadStarted();
  return true;
}

bool Thread::WaitUntilThreadStarted() const {
  DCHECK(owning_sequence_checker_.CalledOnValidSequence());
  if (!message_loop_)
    return false;
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  start_event_.Wait();
  return true;
}

void Thread::FlushForTesting() {
  DCHECK(owning_sequence_checker_.CalledOnValidSequence());
  if (!message_loop_)
    return;

  WaitableEvent done(WaitableEvent::ResetPolicy::AUTOMATIC,
                     WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner()->PostTask(FROM_HERE,
                          BindOnce(&WaitableEvent::Signal, Unretained(&done)));
  done.Wait();
}

void Thread::Stop() {
  DCHECK(joinable_);

  // TODO(gab): Fix improper usage of this API (http://crbug.com/629139) and
  // enable this check, until then synchronization with Start() via
  // |thread_lock_| is required...
  // DCHECK(owning_sequence_checker_.CalledOnValidSequence());
  AutoLock lock(thread_lock_);

  StopSoon();

  // Can't join if the |thread_| is either already gone or is non-joinable.
  if (thread_.is_null())
    return;

  // Wait for the thread to exit.
  //
  // TODO(darin): Unfortunately, we need to keep |message_loop_| around until
  // the thread exits.  Some consumers are abusing the API.  Make them stop.
  //
  PlatformThread::Join(thread_);
  thread_ = base::PlatformThreadHandle();

  // The thread should nullify |message_loop_| on exit (note: Join() adds an
  // implicit memory barrier and no lock is thus required for this check).
  DCHECK(!message_loop_);

  stopping_ = false;
}

void Thread::StopSoon() {
  // TODO(gab): Fix improper usage of this API (http://crbug.com/629139) and
  // enable this check.
  // DCHECK(owning_sequence_checker_.CalledOnValidSequence());

  if (stopping_ || !message_loop_)
    return;

  stopping_ = true;

  if (using_external_message_loop_) {
    // Setting |stopping_| to true above should have been sufficient for this
    // thread to be considered "stopped" per it having never set its |running_|
    // bit by lack of its own ThreadMain.
    DCHECK(!IsRunning());
    message_loop_ = nullptr;
    return;
  }

  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Thread::ThreadQuitHelper, Unretained(this)));
}

void Thread::DetachFromSequence() {
  DCHECK(owning_sequence_checker_.CalledOnValidSequence());
  owning_sequence_checker_.DetachFromSequence();
}

PlatformThreadId Thread::GetThreadId() const {
  // If the thread is created but not started yet, wait for |id_| being ready.
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  id_event_.Wait();
  return id_;
}

bool Thread::IsRunning() const {
  // TODO(gab): Fix improper usage of this API (http://crbug.com/629139) and
  // enable this check.
  // DCHECK(owning_sequence_checker_.CalledOnValidSequence());

  // If the thread's already started (i.e. |message_loop_| is non-null) and not
  // yet requested to stop (i.e. |stopping_| is false) we can just return true.
  // (Note that |stopping_| is touched only on the same sequence that starts /
  // started the new thread so we need no locking here.)
  if (message_loop_ && !stopping_)
    return true;
  // Otherwise check the |running_| flag, which is set to true by the new thread
  // only while it is inside Run().
  AutoLock lock(running_lock_);
  return running_;
}

void Thread::Run(RunLoop* run_loop) {
  // Overridable protected method to be called from our |thread_| only.
  DCHECK(id_event_.IsSignaled());
  DCHECK_EQ(id_, PlatformThread::CurrentId());

  run_loop->Run();
}

// static
void Thread::SetThreadWasQuitProperly(bool flag) {
  lazy_tls_bool.Pointer()->Set(flag);
}

// static
bool Thread::GetThreadWasQuitProperly() {
  bool quit_properly = true;
#ifndef NDEBUG
  quit_properly = lazy_tls_bool.Pointer()->Get();
#endif
  return quit_properly;
}

void Thread::SetMessageLoop(MessageLoop* message_loop) {
  DCHECK(owning_sequence_checker_.CalledOnValidSequence());
  DCHECK(message_loop);

  // Setting |message_loop_| should suffice for this thread to be considered
  // as "running", until Stop() is invoked.
  DCHECK(!IsRunning());
  message_loop_ = message_loop;
  DCHECK(IsRunning());

  using_external_message_loop_ = true;
}

void Thread::ThreadMain() {
  // First, make GetThreadId() available to avoid deadlocks. It could be called
  // any place in the following thread initialization code.
  DCHECK(!id_event_.IsSignaled());
  // Note: this read of |id_| while |id_event_| isn't signaled is exceptionally
  // okay because ThreadMain has a happens-after relationship with the other
  // write in StartWithOptions().
  DCHECK_EQ(kInvalidThreadId, id_);
  id_ = PlatformThread::CurrentId();
  DCHECK_NE(kInvalidThreadId, id_);
  id_event_.Signal();

  // Complete the initialization of our Thread object.
  PlatformThread::SetName(name_.c_str());
  ANNOTATE_THREAD_NAME(name_.c_str());  // Tell the name to race detector.

  if (sequence_manager_) {
    // Bind the SequenceManager before binding the MessageLoop, so that the
    // TaskQueues are bound before the MessageLoop. This is required as one of
    // the TaskQueues may have already replaced the MessageLoop's TaskRunner,
    // and the MessageLoop's TaskRunner needs to be associated with this thread
    // when we call MessageLoop::BindToCurrentThread().
    sequence_manager_->BindToCurrentThread();
  }

  // Lazily initialize the |message_loop| so that it can run on this thread.
  DCHECK(message_loop_);
  std::unique_ptr<MessageLoop> message_loop(message_loop_);
  message_loop_->BindToCurrentThread();
  message_loop_->SetTimerSlack(message_loop_timer_slack_);

  if (sequence_manager_) {
    sequence_manager_->CompleteInitializationOnBoundThread();
  }

#if defined(OS_POSIX) && !defined(OS_NACL)
  // Allow threads running a MessageLoopForIO to use FileDescriptorWatcher API.
  std::unique_ptr<FileDescriptorWatcher> file_descriptor_watcher;
  if (MessageLoopForIO::IsCurrent()) {
    file_descriptor_watcher.reset(
        new FileDescriptorWatcher(message_loop_->task_runner()));
  }
#endif

#if defined(OS_WIN)
  std::unique_ptr<win::ScopedCOMInitializer> com_initializer;
  if (com_status_ != NONE) {
    com_initializer.reset((com_status_ == STA) ?
        new win::ScopedCOMInitializer() :
        new win::ScopedCOMInitializer(win::ScopedCOMInitializer::kMTA));
  }
#endif

  // Let the thread do extra initialization.
  Init();

  {
    AutoLock lock(running_lock_);
    running_ = true;
  }

  start_event_.Signal();

  RunLoop run_loop;
  run_loop_ = &run_loop;
  Run(run_loop_);

  {
    AutoLock lock(running_lock_);
    running_ = false;
  }

  // Let the thread do extra cleanup.
  CleanUp();

#if defined(OS_WIN)
  com_initializer.reset();
#endif

  sequence_manager_.reset();

  if (message_loop->type() != MessageLoop::TYPE_CUSTOM) {
    // Assert that RunLoop::QuitWhenIdle was called by ThreadQuitHelper. Don't
    // check for custom message pumps, because their shutdown might not allow
    // this.
    DCHECK(GetThreadWasQuitProperly());
  }

  // We can't receive messages anymore.
  // (The message loop is destructed at the end of this block)
  message_loop_ = nullptr;
  run_loop_ = nullptr;
}

void Thread::ThreadQuitHelper() {
  DCHECK(run_loop_);
  run_loop_->QuitWhenIdle();
  SetThreadWasQuitProperly(true);
}

}  // namespace base
