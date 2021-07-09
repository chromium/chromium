// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CHECK_OP_H_
#define BASE_CHECK_OP_H_

#include <cstddef>
#include <type_traits>

#include "base/check.h"
#include "base/template_util.h"

// This header defines the (DP)CHECK_EQ etc. macros.
//
// (DP)CHECK_EQ(x, y) is similar to (DP)CHECK(x == y) but will also log the
// values of x and y if the condition doesn't hold. This works for basic types
// and types with an operator<< or .ToString() method.
//
// The operands are evaluated exactly once, and even in build modes where e.g.
// DCHECK is disabled, the operands and their stringification methods are still
// referenced to avoid warnings about unused variables or functions.
//
// To support the stringification of the check operands, this header is
// *significantly* larger than base/check.h, so it should be avoided in common
// headers.
//
// This header also provides the (DP)CHECK macros (by including check.h), so if
// you use e.g. both CHECK_EQ and CHECK, including this header is enough. If you
// only use CHECK however, please include the smaller check.h instead.

namespace logging {

// Functions for turning check operand values into strings.
// Caller takes ownership of the returned string.
BASE_EXPORT char* CheckOpValueStr(int v);
BASE_EXPORT char* CheckOpValueStr(unsigned v);
BASE_EXPORT char* CheckOpValueStr(long v);
BASE_EXPORT char* CheckOpValueStr(unsigned long v);
BASE_EXPORT char* CheckOpValueStr(long long v);
BASE_EXPORT char* CheckOpValueStr(unsigned long long v);
BASE_EXPORT char* CheckOpValueStr(const void* v);
BASE_EXPORT char* CheckOpValueStr(std::nullptr_t v);
BASE_EXPORT char* CheckOpValueStr(double v);

// Convert a streamable value to string out-of-line to avoid <sstream>.
BASE_EXPORT char* StreamValToStr(const void* v,
                                 void (*stream_func)(std::ostream&,
                                                     const void*));

#ifdef __has_builtin
#define SUPPORTS_BUILTIN_ADDRESSOF (__has_builtin(__builtin_addressof))
#else
#define SUPPORTS_BUILTIN_ADDRESSOF 0
#endif

template <typename T>
inline typename std::enable_if<
    base::internal::SupportsOstreamOperator<const T&>::value &&
        !std::is_function<typename std::remove_pointer<T>::type>::value,
    char*>::type
CheckOpValueStr(const T& v) {
  auto f = [](std::ostream& s, const void* p) {
    s << *reinterpret_cast<const T*>(p);
  };

  // operator& might be overloaded, so do the std::addressof dance.
  // __builtin_addressof is preferred since it also handles Obj-C ARC pointers.
  // Some casting is still needed, because T might be volatile.
#if SUPPORTS_BUILTIN_ADDRESSOF
  const void* vp = const_cast<const void*>(
      reinterpret_cast<const volatile void*>(__builtin_addressof(v)));
#else
  const void* vp = reinterpret_cast<const void*>(
      const_cast<const char*>(&reinterpret_cast<const volatile char&>(v)));
#endif
  return StreamValToStr(vp, f);
}

#undef SUPPORTS_BUILTIN_ADDRESSOF

// Overload for types that have no operator<< but do have .ToString() defined.
template <typename T>
inline typename std::enable_if<
    !base::internal::SupportsOstreamOperator<const T&>::value &&
        base::internal::SupportsToString<const T&>::value,
    char*>::type
CheckOpValueStr(const T& v) {
  // .ToString() may not return a std::string, e.g. blink::WTF::String.
  return CheckOpValueStr(v.ToString());
}

// Provide an overload for functions and function pointers. Function pointers
// don't implicitly convert to void* but do implicitly convert to bool, so
// without this function pointers are always printed as 1 or 0. (MSVC isn't
// standards-conforming here and converts function pointers to regular
// pointers, so this is a no-op for MSVC.)
template <typename T>
inline typename std::enable_if<
    std::is_function<typename std::remove_pointer<T>::type>::value,
    char*>::type
CheckOpValueStr(const T& v) {
  return CheckOpValueStr(reinterpret_cast<const void*>(v));
}

// We need overloads for enums that don't support operator<<.
// (i.e. scoped enums where no operator<< overload was declared).
template <typename T>
inline typename std::enable_if<
    !base::internal::SupportsOstreamOperator<const T&>::value &&
        std::is_enum<T>::value,
    char*>::type
CheckOpValueStr(const T& v) {
  return CheckOpValueStr(
      static_cast<typename std::underlying_type<T>::type>(v));
}

// Captures the result of a CHECK_op and facilitates testing as a boolean.
class CheckOpResult {
 public:
  // An empty result signals success.
  constexpr CheckOpResult() {}

  // A non-success result. expr_str is something like "foo != bar". v1_str and
  // v2_str are the stringified run-time values of foo and bar. Takes ownership
  // of v1_str and v2_str.
  BASE_EXPORT CheckOpResult(const char* expr_str, char* v1_str, char* v2_str);

  // Returns true if the check succeeded.
  constexpr explicit operator bool() const { return !message_; }

  friend class CheckError;

 private:
  char* message_ = nullptr;
};

#if defined(OFFICIAL_BUILD) && defined(NDEBUG)

// Discard log strings to reduce code bloat.
#define CHECK_OP(name, op, val1, val2) CHECK((val1)op(val2))

#else

// Helper macro for binary operators.
// The 'switch' is used to prevent the 'else' from being ambiguous when the
// macro is used in an 'if' clause such as:
// if (a == 1)
//   CHECK_EQ(2, a);
#define CHECK_OP(name, op, val1, val2)                                    \
  switch (0)                                                              \
  case 0:                                                                 \
  default:                                                                \
    if (::logging::CheckOpResult true_if_passed =                         \
            ::logging::Check##name##Impl((val1), (val2),                  \
                                         #val1 " " #op " " #val2))        \
      ;                                                                   \
    else                                                                  \
      ::logging::CheckError::CheckOp(__FILE__, __LINE__, &true_if_passed) \
          .stream()

#endif

// The second overload avoids address-taking of static members for
// fundamental types.
#define DEFINE_CHECK_OP_IMPL(name, op)                                         \
  template <typename T, typename U,                                            \
            std::enable_if_t<!std::is_fundamental<T>::value ||                 \
                                 !std::is_fundamental<U>::value,               \
                             int> = 0>                                         \
  constexpr ::logging::CheckOpResult Check##name##Impl(                        \
      const T& v1, const U& v2, const char* expr_str) {                        \
    if (ANALYZER_ASSUME_TRUE(v1 op v2))                                        \
      return ::logging::CheckOpResult();                                       \
    return ::logging::CheckOpResult(expr_str, CheckOpValueStr(v1),             \
                                    CheckOpValueStr(v2));                      \
  }                                                                            \
  template <typename T, typename U,                                            \
            std::enable_if_t<std::is_fundamental<T>::value &&                  \
                                 std::is_fundamental<U>::value,                \
                             int> = 0>                                         \
  constexpr ::logging::CheckOpResult Check##name##Impl(T v1, U v2,             \
                                                       const char* expr_str) { \
    if (ANALYZER_ASSUME_TRUE(v1 op v2))                                        \
      return ::logging::CheckOpResult();                                       \
    return ::logging::CheckOpResult(expr_str, CheckOpValueStr(v1),             \
                                    CheckOpValueStr(v2));                      \
  }

// clang-format off
DEFINE_CHECK_OP_IMPL(EQ, ==)
DEFINE_CHECK_OP_IMPL(NE, !=)
DEFINE_CHECK_OP_IMPL(LE, <=)
DEFINE_CHECK_OP_IMPL(LT, < )
DEFINE_CHECK_OP_IMPL(GE, >=)
DEFINE_CHECK_OP_IMPL(GT, > )
#undef DEFINE_CHECK_OP_IMPL
#define CHECK_EQ(val1, val2) CHECK_OP(EQ, ==, val1, val2)
#define CHECK_NE(val1, val2) CHECK_OP(NE, !=, val1, val2)
#define CHECK_LE(val1, val2) CHECK_OP(LE, <=, val1, val2)
#define CHECK_LT(val1, val2) CHECK_OP(LT, < , val1, val2)
#define CHECK_GE(val1, val2) CHECK_OP(GE, >=, val1, val2)
#define CHECK_GT(val1, val2) CHECK_OP(GT, > , val1, val2)
// clang-format on

#if DCHECK_IS_ON()

#define DCHECK_OP(name, op, val1, val2)                                    \
  switch (0)                                                               \
  case 0:                                                                  \
  default:                                                                 \
    if (::logging::CheckOpResult true_if_passed =                          \
            ::logging::Check##name##Impl((val1), (val2),                   \
                                         #val1 " " #op " " #val2))         \
      ;                                                                    \
    else                                                                   \
      ::logging::CheckError::DCheckOp(__FILE__, __LINE__, &true_if_passed) \
          .stream()

#else

// Don't do any evaluation but still reference the same stuff as when enabled.
#define DCHECK_OP(name, op, val1, val2)                      \
  EAT_CHECK_STREAM_PARAMS((::logging::CheckOpValueStr(val1), \
                           ::logging::CheckOpValueStr(val2), (val1)op(val2)))

#endif

// clang-format off
#define DCHECK_EQ(val1, val2) DCHECK_OP(EQ, ==, val1, val2)
#define DCHECK_NE(val1, val2) DCHECK_OP(NE, !=, val1, val2)
#define DCHECK_LE(val1, val2) DCHECK_OP(LE, <=, val1, val2)
#define DCHECK_LT(val1, val2) DCHECK_OP(LT, < , val1, val2)
#define DCHECK_GE(val1, val2) DCHECK_OP(GE, >=, val1, val2)
#define DCHECK_GT(val1, val2) DCHECK_OP(GT, > , val1, val2)
// clang-format on

}  // namespace logging

#endif  // BASE_CHECK_OP_H_
