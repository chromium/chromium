// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_SCOPED_CLEAR_LAST_ERROR_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_SCOPED_CLEAR_LAST_ERROR_H_

#include <errno.h>

#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/migration_adapter.h"
#include "build/build_config.h"

namespace partition_alloc::internal::base {

// ScopedClearLastError stores and resets the value of thread local error codes
// (errno, GetLastError()), and restores them in the destructor. This is useful
// to avoid side effects on these values in instrumentation functions that
// interact with the OS.

// Common implementation of ScopedClearLastError for all platforms. Use
// ScopedClearLastError instead.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) ScopedClearLastErrorBase {
 public:
  ScopedClearLastErrorBase() : last_errno_(errno) { errno = 0; }
  ScopedClearLastErrorBase(const ScopedClearLastErrorBase&) = delete;
  ScopedClearLastErrorBase& operator=(const ScopedClearLastErrorBase&) = delete;
  ~ScopedClearLastErrorBase() { errno = last_errno_; }

 private:
  const int last_errno_;
};

#if BUILDFLAG(IS_WIN)

// Windows specific implementation of ScopedClearLastError.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) ScopedClearLastError
    : public ScopedClearLastErrorBase {
 public:
  ScopedClearLastError();
  ScopedClearLastError(const ScopedClearLastError&) = delete;
  ScopedClearLastError& operator=(const ScopedClearLastError&) = delete;
  ~ScopedClearLastError();

 private:
  const unsigned long last_system_error_;
};

#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

using ScopedClearLastError = ScopedClearLastErrorBase;

#endif  // BUILDFLAG(IS_WIN)

}  // namespace partition_alloc::internal::base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_SCOPED_CLEAR_LAST_ERROR_H_
