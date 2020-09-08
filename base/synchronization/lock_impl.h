// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_LOCK_IMPL_H_
#define BASE_SYNCHRONIZATION_LOCK_IMPL_H_

#include "base/base_export.h"
#include "base/check_op.h"
#include "base/macros.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/windows_types.h"
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <ostream>
#endif

namespace base {
class Lock;
class ConditionVariable;

namespace win {
namespace internal {
class AutoNativeLock;
class ScopedHandleVerifier;
}  // namespace internal
}  // namespace win

namespace internal {

// This class implements the underlying platform-specific spin-lock mechanism
// used for the Lock class. Do not use, use Lock instead.
class BASE_EXPORT LockImpl {
  friend class base::Lock;
  friend class base::ConditionVariable;
  friend class base::win::internal::AutoNativeLock;
  friend class base::win::internal::ScopedHandleVerifier;

#if defined(OS_WIN)
  using NativeHandle = CHROME_SRWLOCK;
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  using NativeHandle = pthread_mutex_t;
#endif

  LockImpl();
  ~LockImpl();

  // If the lock is not held, take it and return true.  If the lock is already
  // held by something else, immediately return false.
  inline bool Try();

  // Take the lock, blocking until it is available if necessary.
  inline void Lock();

  // Release the lock.  This must only be called by the lock's holder: after
  // a successful call to Try, or a call to Lock.
  inline void Unlock();

  // Return the native underlying lock.
  // TODO(awalker): refactor lock and condition variables so that this is
  // unnecessary.
  NativeHandle* native_handle() { return &native_handle_; }

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  // Whether this lock will attempt to use priority inheritance.
  static bool PriorityInheritanceAvailable();
#endif

  void LockInternalWithTracking();
  NativeHandle native_handle_;

  DISALLOW_COPY_AND_ASSIGN(LockImpl);
};

void LockImpl::Lock() {
  // The ScopedLockAcquireActivity in LockInternalWithTracking() (not inlined
  // here because of circular includes) is relatively expensive and so its
  // actions can become significant due to the very large number of locks that
  // tend to be used throughout the build. It is also not needed unless the lock
  // is contended.
  //
  // To avoid this cost in the vast majority of the calls, simply "try" the lock
  // first and only do the (tracked) blocking call if that fails. |Try()| is
  // cheap on platforms with futex-type locks, as it doesn't call into the
  // kernel.
  if (LIKELY(Try()))
    return;

  LockInternalWithTracking();
}

#if defined(OS_WIN)
bool LockImpl::Try() {
  return !!::TryAcquireSRWLockExclusive(
      reinterpret_cast<PSRWLOCK>(&native_handle_));
}

void LockImpl::Unlock() {
  ::ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&native_handle_));
}

#elif defined(OS_POSIX) || defined(OS_FUCHSIA)

bool LockImpl::Try() {
  int rv = pthread_mutex_trylock(&native_handle_);
  DCHECK(rv == 0 || rv == EBUSY) << ". " << strerror(rv);
  return rv == 0;
}

void LockImpl::Unlock() {
  int rv = pthread_mutex_unlock(&native_handle_);
  DCHECK_EQ(rv, 0) << ". " << strerror(rv);
}
#endif

// This is an implementation used for AutoLock templated on the lock type.
template <class LockType>
class SCOPED_LOCKABLE BasicAutoLock {
 public:
  struct AlreadyAcquired {};

  explicit BasicAutoLock(LockType& lock) EXCLUSIVE_LOCK_FUNCTION(lock)
      : lock_(lock) {
    lock_.Acquire();
  }

  BasicAutoLock(LockType& lock, const AlreadyAcquired&)
      EXCLUSIVE_LOCKS_REQUIRED(lock)
      : lock_(lock) {
    lock_.AssertAcquired();
  }

  ~BasicAutoLock() UNLOCK_FUNCTION() {
    lock_.AssertAcquired();
    lock_.Release();
  }

 private:
  LockType& lock_;
  DISALLOW_COPY_AND_ASSIGN(BasicAutoLock);
};

// This is an implementation used for AutoUnlock templated on the lock type.
template <class LockType>
class BasicAutoUnlock {
 public:
  explicit BasicAutoUnlock(LockType& lock) : lock_(lock) {
    // We require our caller to have the lock.
    lock_.AssertAcquired();
    lock_.Release();
  }

  ~BasicAutoUnlock() { lock_.Acquire(); }

 private:
  LockType& lock_;
  DISALLOW_COPY_AND_ASSIGN(BasicAutoUnlock);
};

// This is an implementation used for AutoLockMaybe templated on the lock type.
template <class LockType>
class SCOPED_LOCKABLE BasicAutoLockMaybe {
 public:
  explicit BasicAutoLockMaybe(LockType* lock) EXCLUSIVE_LOCK_FUNCTION(lock)
      : lock_(lock) {
    if (lock_)
      lock_->Acquire();
  }

  ~BasicAutoLockMaybe() UNLOCK_FUNCTION() {
    if (lock_) {
      lock_->AssertAcquired();
      lock_->Release();
    }
  }

 private:
  LockType* const lock_;
  DISALLOW_COPY_AND_ASSIGN(BasicAutoLockMaybe);
};

// This is an implementation used for ReleasableAutoLock templated on the lock
// type.
template <class LockType>
class SCOPED_LOCKABLE BasicReleasableAutoLock {
 public:
  explicit BasicReleasableAutoLock(LockType* lock) EXCLUSIVE_LOCK_FUNCTION(lock)
      : lock_(lock) {
    DCHECK(lock_);
    lock_->Acquire();
  }

  ~BasicReleasableAutoLock() UNLOCK_FUNCTION() {
    if (lock_) {
      lock_->AssertAcquired();
      lock_->Release();
    }
  }

  void Release() UNLOCK_FUNCTION() {
    DCHECK(lock_);
    lock_->AssertAcquired();
    lock_->Release();
    lock_ = nullptr;
  }

 private:
  LockType* lock_;
  DISALLOW_COPY_AND_ASSIGN(BasicReleasableAutoLock);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_SYNCHRONIZATION_LOCK_IMPL_H_
