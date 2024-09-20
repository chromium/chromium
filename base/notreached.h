// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NOTREACHED_H_
#define BASE_NOTREACHED_H_

#include "base/base_export.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/logging_buildflags.h"

// TODO(crbug.com/41493641): Remove once NOTIMPLEMENTED() call sites include
// base/notimplemented.h.
#include "base/notimplemented.h"

namespace logging {

// Migration in progress: For new code use NOTREACHED() or
// NOTREACHED(base::NotFatalUntil::M*). NOTREACHED_IN_MIGRATION() is equally
// fatal to NOTREACHED() without parameters but not annotated as [[noreturn]].
#if CHECK_WILL_STREAM() || BUILDFLAG(ENABLE_LOG_ERROR_NOT_REACHED)
#define NOTREACHED_IN_MIGRATION() \
  LOGGING_CHECK_FUNCTION_IMPL(::logging::NotReachedError::NotReached(), false)
#else
#define NOTREACHED_IN_MIGRATION()                          \
  (true) ? ::logging::NotReachedError::TriggerNotReached() \
         : EAT_CHECK_STREAM_PARAMS()
#endif

// Migration in progress: Use NOTREACHED() directly without parameters instead.
// TODO(crbug.com/40580068): Merge this with NOTREACHED().
#if CHECK_WILL_STREAM()
#define NOTREACHED_NORETURN() ::logging::NotReachedNoreturnError()
#else
// This function is used to be able to detect NOTREACHED() failures in stack
// traces where this symbol is preserved (even if inlined). Its implementation
// matches logging::CheckFailure() but intentionally uses a different signature.
[[noreturn]] NOMERGE IMMEDIATE_CRASH_ALWAYS_INLINE void NotReachedFailure() {
  base::ImmediateCrash();
}

#define NOTREACHED_NORETURN() \
  (true) ? ::logging::NotReachedFailure() : EAT_CHECK_STREAM_PARAMS()
#endif

// NOTREACHED() annotates should-be unreachable code. When a base::NotFatalUntil
// milestone is provided the instance is non-fatal (dumps without crashing)
// until that milestone is hit. That is: `NOTREACHED(base::NotFatalUntil::M120)`
// starts crashing in M120. See base/check.h.
#define NOTREACHED(...)                                      \
  BASE_IF(BASE_IS_EMPTY(__VA_ARGS__), NOTREACHED_NORETURN(), \
          LOGGING_CHECK_FUNCTION_IMPL(                       \
              ::logging::NotReachedError::NotReached(__VA_ARGS__), false))

// The DUMP_WILL_BE_NOTREACHED() macro provides a convenient way to
// non-fatally dump in official builds if ever hit. See DUMP_WILL_BE_CHECK for
// suggested usage.
#define DUMP_WILL_BE_NOTREACHED() \
  ::logging::CheckError::DumpWillBeNotReachedNoreturn()

}  // namespace logging

#endif  // BASE_NOTREACHED_H_
