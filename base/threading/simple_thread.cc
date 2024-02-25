// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/simple_thread.h"

#include <memory>
#include <ostream>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"

namespace base {

SimpleThread::SimpleThread(const std::string& name)
    : SimpleThread(name, Options()) {}

SimpleThread::SimpleThread(const std::string& name, const Options& options)
    : name_(name),
      options_(options),
      event_(WaitableEvent::ResetPolicy::MANUAL,
             WaitableEvent::InitialState::NOT_SIGNALED) {}

SimpleThread::~SimpleThread() {
  DCHECK(HasBeenStarted()) << "SimpleThread was never started.";
  DCHECK(!options_.joinable || HasBeenJoined())
      << "Joinable SimpleThread destroyed without being Join()ed.";
}

void SimpleThread::Start() {
  StartAsync();
  ScopedAllowBaseSyncPrimitives allow_wait;
  event_.Wait();  // Wait for the thread to complete initialization.
}

void SimpleThread::Join() {
  DCHECK(options_.joinable) << "A non-joinable thread can't be joined.";
  DCHECK(HasStartBeenAttempted()) << "Tried to Join a never-started thread.";
  DCHECK(!HasBeenJoined()) << "Tried to Join a thread multiple times.";
  BeforeJoin();
  PlatformThread::Join(thread_);
  thread_ = PlatformThreadHandle();
  joined_ = true;
}

void SimpleThread::StartAsync() {
  DCHECK(!HasStartBeenAttempted()) << "Tried to Start a thread multiple times.";
  start_called_ = true;
  BeforeStart();
  bool success =
      options_.joinable
          ? PlatformThread::CreateWithType(options_.stack_size, this, &thread_,
                                           options_.thread_type)
          : PlatformThread::CreateNonJoinableWithType(options_.stack_size, this,
                                                      options_.thread_type);
  CHECK(success);
}

PlatformThreadId SimpleThread::tid() {
  DCHECK(HasBeenStarted());
  return tid_;
}

bool SimpleThread::HasBeenStarted() {
  return event_.IsSignaled();
}

void SimpleThread::ThreadMain() {
  tid_ = PlatformThread::CurrentId();
  PlatformThread::SetName(name_);

  // We've initialized our new thread, signal that we're done to Start().
  event_.Signal();

  BeforeRun();
  Run();
}

DelegateSimpleThread::DelegateSimpleThread(Delegate* delegate,
                                           const std::string& name)
    : DelegateSimpleThread(delegate, name, Options()) {}

DelegateSimpleThread::DelegateSimpleThread(Delegate* delegate,
                                           const std::string& name,
                                           const Options& options)
    : SimpleThread(name, options), delegate_(delegate) {
  DCHECK(delegate_);
}

DelegateSimpleThread::~DelegateSimpleThread() = default;

void DelegateSimpleThread::Run() {
  DCHECK(delegate_) << "Tried to call Run without a delegate (called twice?)";

  // Non-joinable DelegateSimpleThreads are allowed to be deleted during Run().
  // Member state must not be accessed after invoking Run().
  Delegate* delegate = delegate_;
  delegate_ = nullptr;
  delegate->Run();
}

DelegateSimpleThreadPool::DelegateSimpleThreadPool(
    const std::string& name_prefix,
    size_t num_threads)
    : name_prefix_(name_prefix),
      num_threads_(num_threads),
      dry_(WaitableEvent::ResetPolicy::MANUAL,
           WaitableEvent::InitialState::NOT_SIGNALED) {}

DelegateSimpleThreadPool::~DelegateSimpleThreadPool() {
  DCHECK(threads_.empty());
  DCHECK(delegates_.empty());
  DCHECK(!dry_.IsSignaled());
}

void DelegateSimpleThreadPool::Start() {
  DCHECK(threads_.empty()) << "Start() called with outstanding threads.";
  for (size_t i = 0; i < num_threads_; ++i) {
    std::string name(name_prefix_);
    name.push_back('/');
    name.append(NumberToString(i));
    auto thread = std::make_unique<DelegateSimpleThread>(this, name);
    thread->Start();
    threads_.push_back(std::move(thread));
  }
}

void DelegateSimpleThreadPool::JoinAll() {
  DCHECK(!threads_.empty()) << "JoinAll() called with no outstanding threads.";

  // Tell all our threads to quit their worker loop.
  AddWork(nullptr, num_threads_);

  // Join and destroy all the worker threads.
  for (size_t i = 0; i < num_threads_; ++i) {
    threads_[i]->Join();
  }
  threads_.clear();
  DCHECK(delegates_.empty());
}

void DelegateSimpleThreadPool::AddWork(Delegate* delegate,
                                       size_t repeat_count) {
  AutoLock locked(lock_);
  for (size_t i = 0; i < repeat_count; ++i)
    delegates_.push(delegate);
  // If we were empty, signal that we have work now.
  if (!dry_.IsSignaled())
    dry_.Signal();
}

void DelegateSimpleThreadPool::Run() {
  Delegate* work = nullptr;

  while (true) {
    dry_.Wait();
    {
      AutoLock locked(lock_);
      if (!dry_.IsSignaled())
        continue;

      DCHECK(!delegates_.empty());
      work = delegates_.front();
      delegates_.pop();

      // Signal to any other threads that we're currently out of work.
      if (delegates_.empty())
        dry_.Reset();
    }

    // A NULL delegate pointer signals us to quit.
    if (!work)
      break;

    work->Run();
  }
}

}  // namespace base
