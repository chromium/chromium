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
  explicit VoidifyStream(bool ignored) {}

  // This operator has lower precedence than << but higher than ?:
  void operator&(std::ostream&) {}
};

// Helper macro which avoids evaluating the arguents to a stream if the
// condition is false.
#define LAZY_CHECK_STREAM(stream, condition) \
  !(condition) ? (void)0 : ::logging::VoidifyStream() & (stream)

// Macro which uses but does not evaluate expr and any stream parameters.
#define EAT_CHECK_STREAM_PARAMS(expr) \
  true ? (void)0                      \
       : ::logging::VoidifyStream(expr) & (*::logging::g_swallow_stream)
BASE_EXPORT extern std::ostream* g_swallow_stream;

class CheckOpResult;
class LogMessage;

// Class used for raising a check error upon destruction.
class BASE_EXPORT CheckError {
 public:
  static CheckError Check(const char* file, int line, const char* condition);
  static CheckError CheckOp(const char* file, int line, CheckOpResult* result);

  static CheckError DCheck(const char* file, int line, const char* condition);
  static CheckError DCheckOp(const char* file, int line, CheckOpResult* result);

  static CheckError PCheck(const char* file, int line, const char* condition);
  static CheckError PCheck(const char* file, int line);

  static CheckError DPCheck(const char* file, int line, const char* condition);

  static CheckError NotImplemented(const char* file,
                                   int line,
                                   const char* function);

  static CheckError NotReached(const char* file, int line);

  // Stream for adding optional details to the error message.
  std::ostream& stream();

  NOMERGE NOT_TAIL_CALLED ~CheckError();

  CheckError(const CheckError&) = delete;
  CheckError& operator=(const CheckError&) = delete;

 private:
  explicit CheckError(LogMessage* log_message);

  LogMessage* const log_message_;
};

#define CHECK_FUNCTION_IMPL(check_function, condition)                       \
  LAZY_CHECK_STREAM(check_function(__FILE__, __LINE__, #condition).stream(), \
                    !ANALYZER_ASSUME_TRUE(condition))

#if defined(OFFICIAL_BUILD) && defined(NDEBUG) && \
    !BUILDFLAG(DCHECK_IS_CONFIGURABLE)

// Discard log strings to reduce code bloat.
//
// This is not calling BreakDebugger since this is called frequently, and
// calling an out-of-line function instead of a noreturn inline macro prevents
// compiler optimizations.
#define CHECK(condition) \
  UNLIKELY(!(condition)) ? IMMEDIATE_CRASH() : EAT_CHECK_STREAM_PARAMS()

#define CHECK_WILL_STREAM() false

#define PCHECK(condition)                                         \
  LAZY_CHECK_STREAM(                                              \
      ::logging::CheckError::PCheck(__FILE__, __LINE__).stream(), \
      UNLIKELY(!(condition)))

#else

#define CHECK_WILL_STREAM() true

#define CHECK(condition) \
  CHECK_FUNCTION_IMPL(::logging::CheckError::Check, condition)

#define PCHECK(condition) \
  CHECK_FUNCTION_IMPL(::logging::CheckError::PCheck, condition)

#endif

#if DCHECK_IS_ON()

#define DCHECK(condition) \
  CHECK_FUNCTION_IMPL(::logging::CheckError::DCheck, condition)
#define DPCHECK(condition) \
  CHECK_FUNCTION_IMPL(::logging::CheckError::DPCheck, condition)

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
