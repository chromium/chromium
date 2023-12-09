// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NOTREACHED_H_
#define BASE_NOTREACHED_H_

#include "base/base_export.h"
#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/logging_buildflags.h"

namespace logging {

// NOTREACHED() annotates should-be unreachable code. When a base::NotFatalUntil
// milestone is provided the instance is non-fatal (dumps without crashing)
// until that milestone is hit. That is: `NOTREACHED(base::NotFatalUntil::M120)`
// starts crashing in M120. See base/check.h.
//
// Under the kNotReachedIsFatal experiment all NOTREACHED() without a milestone
// argument are fatal. Outside the experiment they dump without crashing. As of
// 2023-06-06 this experiment is disabled everywhere.
//
// TODO(crbug.com/851128): After kNotReachedIsFatal is universally rolled out
// then move callers without a non-fatal milestone argument to
// NOTREACHED_NORETURN(). Then rename the [[noreturn]] version back to
// NOTREACHED().
#if CHECK_WILL_STREAM() || BUILDFLAG(ENABLE_LOG_ERROR_NOT_REACHED)
#define NOTREACHED(...)        \
  LOGGING_CHECK_FUNCTION_IMPL( \
      ::logging::NotReachedError::NotReached(__VA_ARGS__), false)
#else
#define BASE_HAS_VA_ARGS(...) 1
#define NOTREACHED(...)                                            \
  BASE_IF(BASE_IS_EMPTY(__VA_ARGS__),                              \
          (true) ? ::logging::NotReachedError::TriggerNotReached() \
                 : EAT_CHECK_STREAM_PARAMS(),                      \
          LOGGING_CHECK_FUNCTION_IMPL(                             \
              ::logging::NotReachedError::NotReached(__VA_ARGS__), false))
#endif

// NOTREACHED_NORETURN() annotates paths that are supposed to be unreachable.
// They crash if they are ever hit.
// TODO(crbug.com/851128): Rename back to NOTREACHED() once there are no callers
// of the old non-CHECK-fatal macro.
#if CHECK_WILL_STREAM()
#define NOTREACHED_NORETURN() ::logging::NotReachedNoreturnError()
#else
// This function is used to be able to detect NOTREACHED() failures in stack
// traces where this symbol is preserved (even if inlined). Its implementation
// matches logging::CheckFailure() but intentionally uses a different signature.
[[noreturn]] IMMEDIATE_CRASH_ALWAYS_INLINE void NotReachedFailure() {
  base::ImmediateCrash();
}

#define NOTREACHED_NORETURN() \
  (true) ? ::logging::NotReachedFailure() : EAT_CHECK_STREAM_PARAMS()
#endif

// The DUMP_WILL_BE_NOTREACHED_NORETURN() macro provides a convenient way to
// non-fatally dump in official builds if ever hit. See DUMP_WILL_BE_CHECK for
// suggested usage.
#define DUMP_WILL_BE_NOTREACHED_NORETURN() \
  ::logging::CheckError::DumpWillBeNotReachedNoreturn()

// The NOTIMPLEMENTED() macro annotates codepaths which have not been
// implemented yet. If output spam is a serious concern,
// NOTIMPLEMENTED_LOG_ONCE() can be used.
#if DCHECK_IS_ON()
#define NOTIMPLEMENTED() \
  ::logging::CheckError::NotImplemented(__PRETTY_FUNCTION__)

// The lambda returns false the first time it is run, and true every other time.
#define NOTIMPLEMENTED_LOG_ONCE()                                \
  LOGGING_CHECK_FUNCTION_IMPL(NOTIMPLEMENTED(), []() {           \
    bool old_value = true;                                       \
    [[maybe_unused]] static const bool call_once = [](bool* b) { \
      *b = false;                                                \
      return true;                                               \
    }(&old_value);                                               \
    return old_value;                                            \
  }())

#else
#define NOTIMPLEMENTED() EAT_CHECK_STREAM_PARAMS()
#define NOTIMPLEMENTED_LOG_ONCE() EAT_CHECK_STREAM_PARAMS()
#endif

}  // namespace logging

#endif  // BASE_NOTREACHED_H_
