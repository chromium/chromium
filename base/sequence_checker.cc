// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequence_checker.h"

#if DCHECK_IS_ON()
#include <memory>
#include <ostream>

#include "base/check.h"
#include "base/debug/stack_trace.h"
#endif

namespace base {

#if DCHECK_IS_ON()
ScopedValidateSequenceChecker::ScopedValidateSequenceChecker(
    const SequenceChecker& checker) {
  std::unique_ptr<debug::StackTrace> bound_at;
  DCHECK(checker.CalledOnValidSequence(&bound_at))
      << (bound_at ? "\nWas attached to sequence at:\n" + bound_at->ToString()
                   : "");
}

ScopedValidateSequenceChecker::~ScopedValidateSequenceChecker() = default;
#endif  // DCHECK_IS_ON()

}  // namespace base
