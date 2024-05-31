// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_LOCK_SUBTLE_H_
#define BASE_SYNCHRONIZATION_LOCK_SUBTLE_H_

#include "base/auto_reset.h"
#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"

namespace base::subtle {

#if DCHECK_IS_ON()
// Returns addresses of locks held by the current thread. `uintptr_t` is used
// because addresses are meant to be used as unique identifiers but not to be
// dereferenced.
BASE_EXPORT span<const uintptr_t> GetLocksHeldByCurrentThread();
#endif

// Creates a scope in which acquired locks aren't tracked by
// `GetLocksHeldByCurrentThread()`. This is required in rare circumstances where
// the number of locks held simultaneously by a thread may exceed the
// fixed-sized thread-local storage. A lock which isn't tracked by
// `GetLocksHeldByCurrentThread()` cannot be used to satisfy a
// `SequenceChecker`.
class BASE_EXPORT [[maybe_unused]] DoNotTrackLocks {
#if DCHECK_IS_ON()
 public:
  DoNotTrackLocks();
  ~DoNotTrackLocks();

 private:
  AutoReset<bool> auto_reset_;
#else
 public:
  DoNotTrackLocks() = default;
  ~DoNotTrackLocks() = default;
#endif  // DCHECK_IS_ON()
};

}  // namespace base::subtle

#endif  // BASE_SYNCHRONIZATION_LOCK_SUBTLE_H_
