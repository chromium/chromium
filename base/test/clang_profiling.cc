// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/clang_profiling.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"

extern "C" int __llvm_profile_dump(void);

namespace base {

void WriteClangProfilingProfile() {
  // __llvm_profile_dump() guarantees that it will not dump profiling
  // information if it is being called twice or more. However, it is not thread
  // safe, as it is supposed to be called from atexit() handler rather than
  // being called directly from random places. Since we have to call it
  // ourselves, we must ensure thread safety in order to prevent duplication of
  // profiling counters.
  static base::NoDestructor<base::Lock> lock;
  base::AutoLock auto_lock(*lock);

// Fuchsia's profile runtime does not handle profile dumping.
#if !defined(OS_FUCHSIA)
  __llvm_profile_dump();
#endif  // !defined(OS_FUCHSIA)
}

}  // namespace base
