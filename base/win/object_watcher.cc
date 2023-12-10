// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/object_watcher.h"

#include <windows.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"

namespace base {
namespace win {

//-----------------------------------------------------------------------------

ObjectWatcher::ObjectWatcher() = default;

ObjectWatcher::~ObjectWatcher() {
  StopWatching();
}

bool ObjectWatcher::StartWatchingOnce(HANDLE object,
                                      Delegate* delegate,
                                      const Location& from_here) {
  return StartWatchingInternal(object, delegate, true, from_here);
}

bool ObjectWatcher::StartWatchingMultipleTimes(HANDLE object,
                                               Delegate* delegate,
                                               const Location& from_here) {
  return StartWatchingInternal(object, delegate, false, from_here);
}

bool ObjectWatcher::StopWatching() {
  if (!wait_object_)
    return false;

  // Make sure ObjectWatcher is used in a sequenced fashion.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Allow blocking calls for historical reasons; see https://crbug.com/700335.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_blocking;

  // Cancel the wait; blocking on it being unregistered. Note that passing
  // INVALID_HANDLE_VALUE to wait on all callback functions seemlingly waits
  // on other callbacks in the threadpool; not just callbacks from
  // RegisterWaitForSingleObject.
  WaitableEvent event;
  if (!UnregisterWaitEx(wait_object_, event.handle())) {
    // ERROR_IO_PENDING is not a fatal error; see
    // https://learn.microsoft.com/en-us/windows/win32/sync/unregisterwaitex.
    if (const auto error = ::GetLastError(); error != ERROR_IO_PENDING) {
      DPLOG(FATAL) << "UnregisterWaitEx failed";
      return false;
    }
  }
  // Wait for unregistration to complete.
  event.Wait();
  Reset();
  return true;
}

bool ObjectWatcher::IsWatching() const {
  return object_ != nullptr;
}

HANDLE ObjectWatcher::GetWatchedObject() const {
  return object_;
}

// static
void CALLBACK ObjectWatcher::DoneWaiting(void* param, BOOLEAN timed_out) {
  DCHECK(!timed_out);

  // The destructor blocks on any callbacks that are in flight, so we know that
  // that is always a pointer to a valid ObjectWater.
  ObjectWatcher* that = static_cast<ObjectWatcher*>(param);

  // `that` must not be touched once `PostTask` returns since the callback
  // could delete the instance on another thread.
  SequencedTaskRunner* const task_runner = that->task_runner_.get();
  if (that->run_once_) {
    task_runner->PostTask(that->location_, std::move(that->callback_));
  } else {
    task_runner->PostTask(that->location_, that->callback_);
  }
}

bool ObjectWatcher::StartWatchingInternal(HANDLE object,
                                          Delegate* delegate,
                                          bool execute_only_once,
                                          const Location& from_here) {
  DCHECK(delegate);
  DCHECK(!wait_object_) << "Already watching an object";
  DCHECK(SequencedTaskRunner::HasCurrentDefault());

  location_ = from_here;
  task_runner_ = SequencedTaskRunner::GetCurrentDefault();

  run_once_ = execute_only_once;

  // Since our job is to just notice when an object is signaled and report the
  // result back to this sequence, we can just run on a Windows wait thread.
  DWORD wait_flags = WT_EXECUTEINWAITTHREAD;
  if (run_once_)
    wait_flags |= WT_EXECUTEONLYONCE;

  // DoneWaiting can be synchronously called from RegisterWaitForSingleObject,
  // so set up all state now.
  callback_ = BindRepeating(&ObjectWatcher::Signal, weak_factory_.GetWeakPtr(),
                            // For all non-test usages, the delegate's lifetime
                            // exceeds object_watcher's. This should be safe.
                            base::UnsafeDanglingUntriaged(delegate));
  object_ = object;

  if (!RegisterWaitForSingleObject(&wait_object_, object, DoneWaiting, this,
                                   INFINITE, wait_flags)) {
    DPLOG(FATAL) << "RegisterWaitForSingleObject failed";
    Reset();
    return false;
  }

  return true;
}

void ObjectWatcher::Signal(Delegate* delegate) {
  // Signaling the delegate may result in our destruction or a nested call to
  // StartWatching(). As a result, we save any state we need and clear previous
  // watcher state before signaling the delegate.
  HANDLE object = object_;
  if (run_once_)
    StopWatching();
  delegate->OnObjectSignaled(object);
}

void ObjectWatcher::Reset() {
  callback_.Reset();
  location_ = {};
  object_ = nullptr;
  wait_object_ = nullptr;
  task_runner_ = nullptr;
  run_once_ = true;
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace win
}  // namespace base
