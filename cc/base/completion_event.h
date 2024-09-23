// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_COMPLETION_EVENT_H_
#define CC_BASE_COMPLETION_EVENT_H_

#include "base/check.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"

namespace cc {

// Used for making blocking calls from one thread to another. Use only when
// absolutely certain that doing-so will not lead to a deadlock.
//
// It is safe to destroy this object as soon as Wait() returns.
class CompletionEvent {
 public:
  explicit CompletionEvent(base::WaitableEvent::ResetPolicy policy =
                               base::WaitableEvent::ResetPolicy::AUTOMATIC)
      : event_(policy, base::WaitableEvent::InitialState::NOT_SIGNALED) {
#if DCHECK_IS_ON()
    waited_ = false;
    signaled_ = false;
#endif
  }

  ~CompletionEvent() {
#if DCHECK_IS_ON()
    DCHECK(waited_);
    DCHECK(signaled_);
#endif
  }

  void Wait() {
#if DCHECK_IS_ON()
    DCHECK(!waited_);
    waited_ = true;
#endif
    if (IsSignaled()) {
      // The event has already been signaled and cannot be re-signaled.
      // There is a non-trivial amount of machinery in WaitableEvent to quickly
      // return if already signaled, which can be short-circuited.
      return;
    }
    // http://crbug.com/902653
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    event_.Wait();
  }

  bool TimedWait(const base::TimeDelta& max_time) {
#if DCHECK_IS_ON()
    DCHECK(!waited_);
    waited_ = true;
#endif
    // http://crbug.com/902653
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    if (event_.TimedWait(max_time))
      return true;
#if DCHECK_IS_ON()
    waited_ = false;
#endif
    return false;
  }

  bool IsSignaled() { return event_.IsSignaled(); }

  void Signal() {
#if DCHECK_IS_ON()
    DCHECK(!signaled_);
    signaled_ = true;
#endif
    event_.Signal();
  }

 private:
  base::WaitableEvent event_;
#if DCHECK_IS_ON()
  // Used to assert that Wait() and Signal() are each called exactly once.
  bool waited_;
  bool signaled_;
#endif
};

}  // namespace cc

#endif  // CC_BASE_COMPLETION_EVENT_H_
