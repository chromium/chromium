// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_EXPECTED_MACROS_H_
#define BASE_TYPES_EXPECTED_MACROS_H_

#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros/concat.h"
#include "base/macros/remove_parens.h"
#include "base/macros/uniquify.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/types/expected.h"

// Executes an expression `rexpr` that returns a `base::expected<T, E>`. If the
// result is an error, causes the calling function to return. If no additional
// arguments are given, the function return value is the `E` returned by
// `rexpr`. Otherwise, the additional arguments are treated as an invocable that
// expects an E as its last argument and returns some type (including `void`)
// convertible to the function's return type; that is, the function returns the
// result of `std::invoke(..., E)`.
//
// This works with move-only types and can be used in functions that return
// either an `E` directly or a `base::expected<U, E>`, without needing to
// explicitly wrap the return in `base::unexpected`.
//
// # Interface
//
// `RETURN_IF_ERROR(rexpr, ...);`
//
// # Examples
//
// Use with no additional arguments:
// ```
//   SomeErrorCode Foo() {
//     RETURN_IF_ERROR(Function(args...));
//     return SomeErrorCode::kOK;
//   }
// ```
// ```
//   base::expected<int, SomeErrorCode> Bar() {
//     RETURN_IF_ERROR(Function(args...));
//     RETURN_IF_ERROR(obj.Method(args...));
//     return 17;
//   }
// ```
//
// Adjusting the returned error:
// ```
//   RETURN_IF_ERROR(TryProcessing(query),
//                   [](const auto& e) {
//                     return base::StrCat({e, " while processing query"});
//                   });
// ```
//
// Returning a different kind of error:
// ```
//   RETURN_IF_ERROR(TryProcessing(query),
//                   [](auto) { return SomeErrorCode::kFail); });
// ```
//
// Returning void:
// ```
//   RETURN_IF_ERROR(TryProcessing(query), [](auto) {});
// ```
// ```
//   RETURN_IF_ERROR(TryProcessing(query),
//                   [](auto) { LOG(WARNING) << "Uh oh"; }());
// ```
//
// Automatic conversion to `base::expected<U, E>`:
// ```
//   base::expected<int, SomeErrorCode> Foo() {
//     RETURN_IF_ERROR(TryProcessing(query),
//                     [](auto) { return SomeErrorCode::kFail); });
//     return 17;
//   }
// ```
//
// Passing the error to a static/global handler:
// ```
//   RETURN_IF_ERROR(TryProcessing(query), &FailureHandler);
// ```
//
// Passing the error to a handler member function:
// ```
//   RETURN_IF_ERROR(TryProcessing(query), &MyClass::FailureHandler, this);
// ```
#define RETURN_IF_ERROR(rexpr, ...) \
  BASE_INTERNAL_EXPECTED_RETURN_IF_ERROR_IMPL(return, rexpr, __VA_ARGS__)

// Executes an expression `rexpr` that returns a `base::expected<T, E>`. If the
// result is an expected value, moves the `T` into whatever `lhs` defines/refers
// to; otherwise, behaves like RETURN_IF_ERROR() above. Avoid side effects in
// `lhs`, as it will not be evaluated in the error case.
//
// # Interface
//
// `ASSIGN_OR_RETURN(lhs, rexpr, ...);`
//
// WARNING: If `lhs` is parenthesized, the parentheses are removed; for this
//          reason, `lhs` may not contain a ternary (`?:`). See examples for
//          motivation.
//
// WARNING: Expands into multiple statements; cannot be used in a single
//          statement (e.g. as the body of an `if` statement without `{}`)!
//
// # Examples
//
// Declaring and initializing a new variable (ValueType can be anything that can
// be initialized with assignment):
// ```
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(arg));
// ```
//
// Assigning to an existing variable:
// ```
//   ValueType value;
//   ASSIGN_OR_RETURN(value, MaybeGetValue(arg));
// ```
//
// Initializing a `std::unique_ptr`:
// ```
//   ASSIGN_OR_RETURN(std::unique_ptr<T> ptr, MaybeGetPtr(arg));
// ```
//
// Initializing a map. Because of C++ preprocessor limitations, the type used in
// `ASSIGN_OR_RETURN` cannot contain commas, so wrap `lhs` in parentheses:
// ```
//   ASSIGN_OR_RETURN((flat_map<Foo, Bar> my_map), GetMap());
// ```
// Or use `auto` if the type is obvious enough:
// ```
//   ASSIGN_OR_RETURN(auto code_widget, GetCodeWidget());
// ```
//
// Assigning to structured bindings. The same situation with comma as above, so
// wrap `lhs` in parentheses:
// ```
//   ASSIGN_OR_RETURN((auto [first, second]), GetPair());
// ```
//
// Attempting to assign to a ternary will not compile:
// ```
//   ASSIGN_OR_RETURN((cond ? a : b), MaybeGetValue(arg));  // DOES NOT COMPILE
// ```
//
// Adjusting the returned error:
// ```
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query),
//                    [](const auto& e) {
//                      return base::StrCat({e, " while getting value"});
//                    });
// ```
//
// Returning a different kind of error:
// ```
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query),
//                    [](auto) { return SomeErrorCode::kFail); });
// ```
//
// Returning void:
// ```
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query), [](auto) {});
// ```
// ```
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query),
//                    [](auto) { LOG(WARNING) << "Uh oh"; }());
// ```
//
// Automatic conversion to `base::expected<U, E>`:
// ```
//   base::expected<int, SomeErrorCode> Foo() {
//     ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query),
//                      [](auto) { return SomeErrorCode::kFail); });
//     return 17;
//   }
// ```
//
// Passing the error to a static/global handler:
// ```
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query), &FailureHandler);
// ```
//
// Passing the error to a handler member function:
// ```
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query),
//                    &MyClass::FailureHandler, this);
// ```
#define ASSIGN_OR_RETURN(lhs, rexpr, ...) \
  BASE_INTERNAL_EXPECTED_ASSIGN_OR_RETURN_IMPL(return, lhs, rexpr, __VA_ARGS__)

namespace base::internal {

// =================================================================
// == Implementation details, do not rely on anything below here. ==
// =================================================================

// Helper object to allow returning some `E` from a method either directly or in
// the error of an `expected<T, E>`. Supports move-only `E`, as well as `void`.
//
// In order to support `void` return types, `UnexpectedDeducer` is not
// constructed directly from an `E`, but from a lambda that returns `E`; and
// callers must return `Ret()` rather than returning the deducer itself. Using
// both these indirections allows consistent invocation from macros.
template <typename Lambda, typename E = std::invoke_result_t<Lambda&&>>
class UnexpectedDeducer {
 public:
  constexpr explicit UnexpectedDeducer(Lambda&& lambda) noexcept
      : lambda_(std::move(lambda)) {}

  constexpr decltype(auto) Ret() && noexcept {
    if constexpr (std::is_void_v<E>) {
      std::move(lambda_)();
    } else {
      return std::move(*this);
    }
  }

  // Allow implicit conversion from `Ret()` to either `expected<T, E>` (for
  // arbitrary `T`) or `E`.
  template <typename T>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator expected<T, E>() && noexcept {
    return expected<T, E>(unexpect, std::move(lambda_)());
  }
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator E() && noexcept { return std::move(lambda_)(); }

 private:
  // RAW_PTR_EXCLUSION: Not intended to handle &&-qualified members.
  // `UnexpectedDeducer` is a short-lived temporary and tries to minimize
  // copying and other overhead; using raw_ptr/ref goes against this design
  // without adding meaningful safety.
  RAW_PTR_EXCLUSION Lambda&& lambda_;
};

// Deduce the type of the lambda automatically so callers don't need to spell
// things twice (or use temps) and use decltype.
template <typename Lambda>
UnexpectedDeducer(Lambda) -> UnexpectedDeducer<Lambda>;

}  // namespace base::internal

#define BASE_INTERNAL_EXPECTED_BODY(expected, rexpr, name, return_keyword, \
                                    error_expr)                            \
  decltype(auto) expected = (rexpr);                                       \
  {                                                                        \
    static_assert(base::internal::IsExpected<decltype(expected)>,          \
                  #name " should only be used with base::expected<>");     \
  }                                                                        \
  if (!expected.has_value()) [[unlikely]] {                                \
    return_keyword base::internal::UnexpectedDeducer([&] {                 \
      return error_expr;                                                   \
    }).Ret();                                                              \
  }

#define BASE_INTERNAL_EXPECTED_RETURN_IF_ERROR(expected, rexpr,            \
                                               return_keyword, error_expr) \
  do {                                                                     \
    BASE_INTERNAL_EXPECTED_BODY(expected, rexpr, RETURN_IF_ERROR,          \
                                return_keyword, error_expr);               \
  } while (false)

#define BASE_INTERNAL_EXPECTED_ASSIGN_OR_RETURN(                           \
    expected, rexpr, return_keyword, error_expr, lhs)                      \
  BASE_INTERNAL_EXPECTED_BODY(expected, rexpr, ASSIGN_OR_RETURN,           \
                              return_keyword, error_expr);                 \
  {                                                                        \
    constexpr auto lhs_v = std::string_view(#lhs);                         \
    static_assert(!(lhs_v.front() == '(' && lhs_v.back() == ')' &&         \
                    lhs_v.rfind('?') != std::string_view::npos),           \
                  "Identified possible ternary in `lhs`; avoid passing "   \
                  "parenthesized expressions containing '?' to the first " \
                  "argument of ASSIGN_OR_RETURN()");                       \
  }                                                                        \
  BASE_REMOVE_PARENS(lhs) = std::move(expected).value();

#define BASE_INTERNAL_EXPECTED_PASS_ARGS(func, ...) func(__VA_ARGS__)

// These are necessary to avoid mismatched parens inside __VA_OPT__() below.
#define BASE_INTERNAL_EXPECTED_BEGIN_INVOKE std::invoke(
#define BASE_INTERNAL_EXPECTED_END_INVOKE )

#define BASE_INTERNAL_EXPECTED_ARGS(temp_name, return_keyword, rexpr, ...) \
  temp_name, rexpr, return_keyword,                                        \
      (__VA_OPT__(BASE_INTERNAL_EXPECTED_BEGIN_INVOKE)                     \
           __VA_ARGS__ __VA_OPT__(, ) std::move(temp_name)                 \
               .error() __VA_OPT__(BASE_INTERNAL_EXPECTED_END_INVOKE))

#define BASE_INTERNAL_EXPECTED_RETURN_IF_ERROR_IMPL(return_keyword, rexpr, \
                                                    ...)                   \
  BASE_INTERNAL_EXPECTED_PASS_ARGS(                                        \
      BASE_INTERNAL_EXPECTED_RETURN_IF_ERROR,                              \
      BASE_INTERNAL_EXPECTED_ARGS(BASE_UNIQUIFY(_expected_value),          \
                                  return_keyword, rexpr, __VA_ARGS__))

#define BASE_INTERNAL_EXPECTED_ASSIGN_OR_RETURN_IMPL(return_keyword, lhs, \
                                                     rexpr, ...)          \
  BASE_INTERNAL_EXPECTED_PASS_ARGS(                                       \
      BASE_INTERNAL_EXPECTED_ASSIGN_OR_RETURN,                            \
      BASE_INTERNAL_EXPECTED_ARGS(BASE_UNIQUIFY(_expected_value),         \
                                  return_keyword, rexpr, __VA_ARGS__),    \
      lhs)

#endif  // BASE_TYPES_EXPECTED_MACROS_H_
