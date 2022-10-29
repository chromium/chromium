// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_CHECK_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_CHECK_H_

#include <iosfwd>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_base/immediate_crash.h"

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

namespace partition_alloc::internal::logging {

// Class used to explicitly ignore an ostream, and optionally a boolean value.
class VoidifyStream {
 public:
  VoidifyStream() = default;
  explicit VoidifyStream(bool ignored) {}

  // This operator has lower precedence than << but higher than ?:
  void operator&(std::ostream&) {}
};

// Helper macro which avoids evaluating the arguments to a stream if the
// condition is false.
#define PA_LAZY_CHECK_STREAM(stream, condition) \
  !(condition)                                  \
      ? (void)0                                 \
      : ::partition_alloc::internal::logging::VoidifyStream() & (stream)

// Macro which uses but does not evaluate expr and any stream parameters.
#define PA_EAT_CHECK_STREAM_PARAMS(expr)                             \
  true ? (void)0                                                     \
       : ::partition_alloc::internal::logging::VoidifyStream(expr) & \
             (*::partition_alloc::internal::logging::g_swallow_stream)
PA_COMPONENT_EXPORT(PARTITION_ALLOC) extern std::ostream* g_swallow_stream;

class LogMessage;

// Class used for raising a check error upon destruction.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) CheckError {
 public:
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

  PA_NOMERGE ~CheckError();

  CheckError(const CheckError& other) = delete;
  CheckError& operator=(const CheckError& other) = delete;
  CheckError(CheckError&& other) = default;
  CheckError& operator=(CheckError&& other) = default;

 private:
  explicit CheckError(LogMessage* log_message);

  LogMessage* log_message_;
};

#if defined(OFFICIAL_BUILD) && !defined(NDEBUG)
#error "Debug builds are not expected to be optimized as official builds."
#endif  // defined(OFFICIAL_BUILD) && !defined(NDEBUG)

#if defined(OFFICIAL_BUILD) && !BUILDFLAG(PA_DCHECK_IS_ON)

// Discard log strings to reduce code bloat.
//
// This is not calling BreakDebugger since this is called frequently, and
// calling an out-of-line function instead of a noreturn inline macro prevents
// compiler optimizations.
#define PA_BASE_CHECK(condition)                   \
  PA_UNLIKELY(!(condition)) ? PA_IMMEDIATE_CRASH() \
                            : PA_EAT_CHECK_STREAM_PARAMS()

// TODO(1151236): base/test/gtest_util.h uses CHECK_WILL_STREAM(). After
// copying (or removing) gtest_util.h and removing gtest_uti.h from partition
// allocator's DEPS, rename or remove CHECK_WILL_STREAM().
#define CHECK_WILL_STREAM() false

#define PA_BASE_PCHECK(condition)                                        \
  PA_LAZY_CHECK_STREAM(                                                  \
      ::partition_alloc::internal::logging::CheckError::PCheck(__FILE__, \
                                                               __LINE__) \
          .stream(),                                                     \
      PA_UNLIKELY(!(condition)))

#else

#define PA_BASE_CHECK(condition)                               \
  PA_LAZY_CHECK_STREAM(                                        \
      ::partition_alloc::internal::logging::CheckError::Check( \
          __FILE__, __LINE__, #condition)                      \
          .stream(),                                           \
      !PA_ANALYZER_ASSUME_TRUE(condition))

#define CHECK_WILL_STREAM() true

#define PA_BASE_PCHECK(condition)                               \
  PA_LAZY_CHECK_STREAM(                                         \
      ::partition_alloc::internal::logging::CheckError::PCheck( \
          __FILE__, __LINE__, #condition)                       \
          .stream(),                                            \
      !PA_ANALYZER_ASSUME_TRUE(condition))

#endif

#if BUILDFLAG(PA_DCHECK_IS_ON)

#define PA_BASE_DCHECK(condition)                               \
  PA_LAZY_CHECK_STREAM(                                         \
      ::partition_alloc::internal::logging::CheckError::DCheck( \
          __FILE__, __LINE__, #condition)                       \
          .stream(),                                            \
      !PA_ANALYZER_ASSUME_TRUE(condition))

#define PA_BASE_DPCHECK(condition)                               \
  PA_LAZY_CHECK_STREAM(                                          \
      ::partition_alloc::internal::logging::CheckError::DPCheck( \
          __FILE__, __LINE__, #condition)                        \
          .stream(),                                             \
      !PA_ANALYZER_ASSUME_TRUE(condition))

#else

#define PA_BASE_DCHECK(condition) PA_EAT_CHECK_STREAM_PARAMS(!(condition))
#define PA_BASE_DPCHECK(condition) PA_EAT_CHECK_STREAM_PARAMS(!(condition))

#endif

// Async signal safe checking mechanism.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) void RawCheck(const char* message);
PA_COMPONENT_EXPORT(PARTITION_ALLOC) void RawError(const char* message);
#define PA_RAW_CHECK(condition)                       \
  do {                                                \
    if (!(condition))                                 \
      ::partition_alloc::internal::logging::RawCheck( \
          "Check failed: " #condition "\n");          \
  } while (0)

}  // namespace partition_alloc::internal::logging

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_CHECK_H_
