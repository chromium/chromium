// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_checker.h"

#if DCHECK_IS_ON()
#include <memory>
#include <ostream>
#include <string_view>

#include "base/check.h"
#include "base/debug/stack_trace.h"
#endif

namespace base {

#if DCHECK_IS_ON()
ScopedValidateThreadChecker::ScopedValidateThreadChecker(
    const ThreadChecker& checker) {
  std::unique_ptr<debug::StackTrace> bound_at;
  DCHECK(checker.CalledOnValidThread(&bound_at))
      << (bound_at ? "\nWas attached to thread at:\n" + bound_at->ToString()
                   : "");
}

ScopedValidateThreadChecker::ScopedValidateThreadChecker(
    const ThreadChecker& checker,
    std::string_view msg) {
  std::unique_ptr<debug::StackTrace> bound_at;
  DCHECK(checker.CalledOnValidThread(&bound_at))
      << msg
      << (bound_at ? "\nWas attached to thread at:\n" + bound_at->ToString()
                   : "");
}

ScopedValidateThreadChecker::~ScopedValidateThreadChecker() = default;
#endif  // DCHECK_IS_ON()

}  // namespace base
