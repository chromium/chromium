// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CHECK_H_
#define BASE_CHECK_H_

#include <iosfwd>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/debug/debugging_buildflags.h"
#include "base/immediate_crash.h"

// This header defines the CHECK, DCHECK, and DPCHECK macros.
//
// CHECK dies with a fatal error if its condition is not true. It is not
// controlled by NDEBUG, so the check will be executed regardless of compilation
// mode.
//
// DCHECK, the "debug mode" check, is enabled depending on NDEBUG and
// DCHECK_ALWAYS_ON, and its severity depends on DCHECK_IS_CONFIGURABLE.
//
// (D)PCHECK is like (D)CHECK, but includes the system error code (c.f.
// perror(3)).
//
// Additional information can be streamed to these macros and will be included
// in the log output if the condition doesn't hold (you may need to include
// <ostream>):
//
//   CHECK(condition) << "Additional info.";
//
// The condition is evaluated exactly once. Even in build modes where e.g.
// DCHECK is disabled, the condition and any stream arguments are still
// referenced to avoid warnings about unused variables and functions.
//
// For the (D)CHECK_EQ, etc. macros, see base/check_op.h. However, that header
// is *significantly* larger than check.h, so try to avoid including it in
// header files.

namespace logging {

// Class used to explicitly ignore an ostream, and optionally a boolean value.
class VoidifyStream {
 public:
  VoidifyStream() = default;
  explicit VoidifyStream(bool) {}

  // This operator has lower precedence than << but higher than ?:
  void operator&(std::ostream&) {}
};

// Macro which uses but does not evaluate expr and any stream parameters.
#define EAT_CHECK_STREAM_PARAMS(expr) \
  true ? (void)0                      \
       : ::logging::VoidifyStream(expr) & (*::logging::g_swallow_stream)
BASE_EXPORT extern std::ostream* g_swallow_stream;

class LogMessage;

// Class used for raising a check error upon destruction.
class BASE_EXPORT CheckError {
 public:
  // Used by CheckOp. Takes ownership of `log_message`.
  explicit CheckError(LogMessage* log_message) : log_message_(log_message) {}

  static CheckError Check(const char* file, int line, const char* condition);

  static CheckError DCheck(const char* file, int line, const char* condition);

  static CheckError PCheck(const char* file, int line, const char* condition);
  static CheckError PCheck(const char* file, int line);

  static CheckError DPCheck(const char* file, int line, const char* condition);

  static CheckError NotImplemented(const char* file,
                                   int line,
                                   const char* function);

  // Stream for adding optional details to the error message.
  std::ostream& stream();

  // Try really hard to get the call site and callee as separate stack frames in
  // crash reports.
  NOMERGE NOINLINE NOT_TAIL_CALLED ~CheckError();

  CheckError(const CheckError&) = delete;
  CheckError& operator=(const CheckError&) = delete;

  template <typename T>
  std::ostream& operator<<(T&& streamed_type) {
    return stream() << streamed_type;
  }

 protected:
  LogMessage* const log_message_;
};

class BASE_EXPORT NotReachedError : public CheckError {
 public:
  static NotReachedError NotReached(const char* file, int line);

  // Used to trigger a NOTREACHED() without providing file or line while also
  // discarding log-stream arguments. See base/notreached.h.
  NOMERGE NOINLINE NOT_TAIL_CALLED static void TriggerNotReached();

  // TODO(crbug.com/851128): Mark [[noreturn]] once this is CHECK-fatal on all
  // builds.
  NOMERGE NOINLINE NOT_TAIL_CALLED ~NotReachedError();

 private:
  using CheckError::CheckError;
};

// TODO(crbug.com/851128): This should take the name of the above class once all
// callers of NOTREACHED() have migrated to the CHECK-fatal version.
class BASE_EXPORT NotReachedNoreturnError : public CheckError {
 public:
  NotReachedNoreturnError(const char* file, int line);

  [[noreturn]] NOMERGE NOINLINE NOT_TAIL_CALLED ~NotReachedNoreturnError();
};

// The 'switch' is used to prevent the 'else' from being ambiguous when the
// macro is used in an 'if' clause such as:
// if (a == 1)
//   CHECK(Foo());
//
// TODO(crbug.com/1380930): Remove the const bool when the blink-gc plugin has
// been updated to accept `if (LIKELY(!field_))` as well as `if (!field_)`.
#define CHECK_FUNCTION_IMPL(check_failure_invocation, condition)   \
  switch (0)                                                       \
  case 0:                                                          \
  default:                                                         \
    if (const bool checky_bool_lol = static_cast<bool>(condition); \
        LIKELY(ANALYZER_ASSUME_TRUE(checky_bool_lol)))             \
      ;                                                            \
    else                                                           \
      check_failure_invocation

#if defined(OFFICIAL_BUILD) && !defined(NDEBUG)
#error "Debug builds are not expected to be optimized as official builds."
#endif  // defined(OFFICIAL_BUILD) && !defined(NDEBUG)

#if defined(OFFICIAL_BUILD) && !DCHECK_IS_ON()
// Note that this uses IMMEDIATE_CRASH_ALWAYS_INLINE to force-inline in debug
// mode as well. See LoggingTest.CheckCausesDistinctBreakpoints.
[[noreturn]] IMMEDIATE_CRASH_ALWAYS_INLINE void CheckFailure() {
  base::ImmediateCrash();
}

// Discard log strings to reduce code bloat.
//
// This is not calling BreakDebugger since this is called frequently, and
// calling an out-of-line function instead of a noreturn inline macro prevents
// compiler optimizations.
#define CHECK(condition) \
  UNLIKELY(!(condition)) ? logging::CheckFailure() : EAT_CHECK_STREAM_PARAMS()

#define CHECK_WILL_STREAM() false

// Strip the conditional string from official builds.
#define PCHECK(condition)                                                \
  CHECK_FUNCTION_IMPL(::logging::CheckError::PCheck(__FILE__, __LINE__), \
                      condition)

#else

#define CHECK_WILL_STREAM() true

#define CHECK(condition) \
  CHECK_FUNCTION_IMPL(   \
      ::logging::CheckError::Check(__FILE__, __LINE__, #condition), condition)

#define PCHECK(condition)                                            \
  CHECK_FUNCTION_IMPL(                                               \
      ::logging::CheckError::PCheck(__FILE__, __LINE__, #condition), \
      condition)

#endif

#if DCHECK_IS_ON()

#define DCHECK(condition)                                            \
  CHECK_FUNCTION_IMPL(                                               \
      ::logging::CheckError::DCheck(__FILE__, __LINE__, #condition), \
      condition)
#define DPCHECK(condition)                                            \
  CHECK_FUNCTION_IMPL(                                                \
      ::logging::CheckError::DPCheck(__FILE__, __LINE__, #condition), \
      condition)

#else

#define DCHECK(condition) EAT_CHECK_STREAM_PARAMS(!(condition))
#define DPCHECK(condition) EAT_CHECK_STREAM_PARAMS(!(condition))

#endif

// Async signal safe checking mechanism.
BASE_EXPORT void RawCheck(const char* message);
BASE_EXPORT void RawError(const char* message);
#define RAW_CHECK(condition)                                 \
  do {                                                       \
    if (!(condition))                                        \
      ::logging::RawCheck("Check failed: " #condition "\n"); \
  } while (0)

}  // namespace logging

#endif  // BASE_CHECK_H_
