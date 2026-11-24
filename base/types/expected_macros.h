// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_EXPECTED_MACROS_H_
#define BASE_TYPES_EXPECTED_MACROS_H_

#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros/concat.h"
#include "base/macros/remove_parens.h"
#include "base/macros/uniquify.h"
#include "base/types/expected.h"
#include "base/types/is_instantiation.h"

// Executes an expression `rexpr` that returns an `expected<T, E>` or
// `std::optional<T>`.
//
// For the `expected<T, E>` case:
//   If the result is an error, causes the calling function to return. If no
//   additional arguments are given, the function return value is the `E`
//   returned by `rexpr`. Otherwise, the additional arguments are one of:
//   * An invocable that expects an E as its last argument and returns some type
//     (including `void`) convertible to the function's return type; that is,
//     the function returns the result of `std::invoke(..., E)` on the
//     additional arguments.
//   * A single value that is not invocable with an E; the function returns that
//     value as `return value;` would. It is only evaluated if the result is an
//     error.
//
//   This works with move-only types. Unless a plain value is given, it can be
//   used in functions that return either an `E` directly or a
//   `base::expected<U, E>`, without needing to explicitly wrap the return in
//   `base::unexpected`. A plain value has to be a `base::unexpected` itself to
//   return an error from such a function.
//
// For the `std::optional<T>` case:
//   If the result is `std::nullopt`, causes the calling function to return. If
//   no additional arguments are given, the function return value is the return
//   value of `rexpr` (i.e. `std::nullopt`). Otherwise, the additional arguments
//   are one of:
//   * An invocable that returns some type (including `void`) convertible to the
//     function's return type; that is, the function returns the result of
//     `std::invoke(...)` on the additional arguments.
//   * A single value that is not invocable; the function returns that value as
//     `return value;` would. It is only evaluated if the result is
//     `std::nullopt`.
//
// In both cases:
// * Function pointers, member pointers and objects with a single, non-template
//   call operator (e.g. lambdas that are not generic) are never treated as a
//   value.
// * It is a compile error if the additional arguments are neither invocable
//   nor a single value.
//
// # Interface
//
// `RETURN_IF_ERROR(rexpr, ...);`
//
// # Examples for the `expected<T, E>` case
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
//   RETURN_IF_ERROR(TryProcessing(query), SomeErrorCode::kFail);
// ```
//
// Returning void:
// ```
//   RETURN_IF_ERROR(TryProcessing(query), [](auto) {});
// ```
// ```
//   RETURN_IF_ERROR(TryProcessing(query),
//                   [](auto) { LOG(WARNING) << "Uh oh"; });
// ```
//
// Automatic conversion to `base::expected<U, E>`:
// ```
//   base::expected<int, SomeErrorCode> Foo() {
//     RETURN_IF_ERROR(TryProcessing(query),
//                     [](auto) { return SomeErrorCode::kFail; });
//     return 17;
//   }
// ```
//
// Returning an error as a plain value from such a function:
// ```
//   base::expected<int, SomeErrorCode> Foo() {
//     RETURN_IF_ERROR(TryProcessing(query),
//                     base::unexpected(SomeErrorCode::kFail));
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
//
// # Modified examples for the `std::optional<T>` case
//
// Use with no additional arguments:
// ```
//   std::optional<int> Foo() {
//     RETURN_IF_ERROR(Function(args...));
//     RETURN_IF_ERROR(obj.Method(args...));
//     return 17;
//   }
// ```
//
// Returning some kind of error:
// ```
//   RETURN_IF_ERROR(TryProcessing(query), SomeErrorCode::kFail);
// ```
//
// Returning void:
// ```
//   RETURN_IF_ERROR(TryProcessing(query), [] {});
// ```
// ```
//   RETURN_IF_ERROR(TryProcessing(query), [] { LOG(WARNING) << "Uh oh"; });
// ```
#define RETURN_IF_ERROR(rexpr, ...)                                        \
  BASE_INTERNAL_EXPECTED_PASS_ARGS(BASE_INTERNAL_EXPECTED_RETURN_IF_ERROR, \
                                   BASE_UNIQUIFY(_expected_value), rexpr,  \
                                   __VA_ARGS__)

// Executes an expression `rexpr` that returns an `expected<T, E>` or
// `std::optional<T>`. If the result is not an error/`std::nullopt`
// (respectively), moves the `T` into whatever `lhs` defines/refers to;
// otherwise, behaves like RETURN_IF_ERROR() above. Avoid side effects in `lhs`,
// as it will not be evaluated in the error case.
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
// # Examples for the `expected<T, E>` case
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
//                    SomeErrorCode::kFail);
// ```
//
// Returning `nullptr` from a function that returns a (smart) pointer:
// ```
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query), nullptr);
// ```
//
// Returning void:
// ```
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query), [](auto) {});
// ```
// ```
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query),
//                    [](auto) { LOG(WARNING) << "Uh oh"; });
// ```
//
// Automatic conversion to `base::expected<U, E>`:
// ```
//   base::expected<int, SomeErrorCode> Foo() {
//     ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query),
//                      [](auto) { return SomeErrorCode::kFail; });
//     return 17;
//   }
// ```
//
// Returning an error as a plain value from such a function:
// ```
//   base::expected<int, SomeErrorCode> Foo() {
//     ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query),
//                      base::unexpected(SomeErrorCode::kFail));
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
//
// # Modified examples for the `std::optional<T>` case
//
// Returning some kind of error:
// ```
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query),
//                    SomeErrorCode::kFail);
// ```
//
// Returning void:
// ```
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query), [] {});
// ```
// ```
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query),
//                    [] { LOG(WARNING) << "Uh oh"; });
// ```
#define ASSIGN_OR_RETURN(lhs, rexpr, ...)                                      \
  BASE_INTERNAL_EXPECTED_PASS_ARGS(BASE_INTERNAL_EXPECTED_ASSIGN_OR_RETURN,    \
                                   lhs, BASE_UNIQUIFY(_expected_value), rexpr, \
                                   __VA_ARGS__)

namespace base::internal_expected_macros {

// =================================================================
// == Implementation details, do not rely on anything below here. ==
// =================================================================

// Whether `T` is something callers pass to the macros as a handler: a function
// pointer, a member pointer, or an object with a single, non-template call
// operator, such as a lambda that is not generic. These are never a value to
// return. Otherwise a handler that can't be invoked with the error would
// convert to `true` in a function that returns `bool`.
template <typename T>
concept IsHandler = std::is_member_pointer_v<T> ||
                    std::is_function_v<std::remove_pointer_t<T>> ||
                    requires { &T::operator(); };

// The handler used when the macros get no additional arguments. This is a
// constant rather than a `std::identity()` temporary at its uses because
// `UnexpectedDeducer` only holds a reference to it and outlives
// `ErrorReturn()`.
inline constexpr std::identity kIdentity{};

// Helper object to allow returning some `E` from a method either directly or in
// the error of an `expected<T, E>`. Defers invoking `Fn` until conversion so
// prvalues of immovable types still get guaranteed copy elision. Only holds
// references to `fn` and `args`, so they have to outlive it.
template <typename Fn, typename... Args>
class UnexpectedDeducer {
  using RawE = std::invoke_result_t<Fn&&, Args&&...>;
  using E = std::remove_cvref_t<RawE>;

 public:
  constexpr explicit UnexpectedDeducer(Fn&& fn, Args&&... args) noexcept
      : call_(std::forward<Fn>(fn), std::forward<Args>(args)...) {}

  // Allow implicit conversion to types that can be converted from
  // `unexpected<E>`, e.g. `base::expected<T, Err>` with arbitrary `T` and
  // compatible `Err`.
  template <typename T, typename Err>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator expected<T, Err>() && noexcept
    requires(std::convertible_to<RawE, Err>)
  {
    return expected<T, Err>(unexpect, std::move(*this).Invoke());
  }

  // Allow implicit conversion from `std::nullopt` to compatible types, e.g.
  // `std::optional<T>` with arbitrary `T`.
  template <typename T>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator T() && noexcept
    requires(std::convertible_to<RawE, T> && std::same_as<E, std::nullopt_t>)
  {
    return std::move(*this).Invoke();
  }

  // Allow implicit conversion from `nullptr` to compatible types, e.g. `T*` or
  // `std::unique_ptr<T>`.
  template <typename T>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator T() && noexcept
    requires(std::convertible_to<RawE, T> && std::same_as<E, std::nullptr_t>)
  {
    return std::move(*this).Invoke();
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator E() && noexcept { return std::move(*this).Invoke(); }

  // Disallow implicit conversion to `std::optional<T>`. Either `E` is already
  // a type that can convert to this and this is unnecessary due to the
  // conversion operator above, or `E` is some other type and we're discarding
  // whatever was in it. Theoretically this might not be an information loss if
  // e.g. `E` is an unbound `std::optional<U>`, but it seems better to force
  // people to match types in this case. Also note that since `E` isn't
  // convertible, this would be a compile error even without deleting this
  // function; but deleting it makes it clear this isn't an omission in this
  // code, but behavior we explicitly don't want to support.
  template <typename T>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator std::optional<T>() && noexcept
    requires(!std::same_as<E, std::optional<T>> &&
             !std::convertible_to<RawE, std::optional<T>>)
  = delete;  // Use an adapter that returns this type.

 private:
  constexpr RawE Invoke() && noexcept {
    return std::apply(
        [](Fn&& fn, Args&&... args) -> RawE {
          return std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
        },
        std::move(call_));
  }

  std::tuple<Fn&&, Args&&...> call_;
};

template <typename Fn, typename... Args>
UnexpectedDeducer(Fn&&, Args&&...) -> UnexpectedDeducer<Fn, Args...>;

// Computes what the macros return for the failed `result`. The handler
// overloads spell out `std::invocable` although `std::invoke_result_t` implies
// it, so that the compiler says why a handler can't be invoked.

// No additional arguments: propagate the error or `std::nullopt`.
template <typename T, typename E>
constexpr auto ErrorReturn(expected<T, E>&& result) noexcept {
  return UnexpectedDeducer{kIdentity, std::move(result).error()};
}

template <typename T>
constexpr auto ErrorReturn(std::optional<T>&&) noexcept {
  return UnexpectedDeducer{kIdentity, std::nullopt};
}

// `void`-returning handler: invoke it and return `void`.
template <typename T, typename E, typename Fn, typename... Args>
  requires std::invocable<Fn&&, Args&&..., E> &&
           std::is_void_v<std::invoke_result_t<Fn&&, Args&&..., E>>
constexpr void ErrorReturn(expected<T, E>&& result,
                           Fn&& fn,
                           Args&&... args) noexcept {
  std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...,
              std::move(result).error());
}

template <typename T, typename Fn, typename... Args>
  requires std::invocable<Fn&&, Args&&...> &&
           std::is_void_v<std::invoke_result_t<Fn&&, Args&&...>>
constexpr void ErrorReturn(std::optional<T>&&,
                           Fn&& fn,
                           Args&&... args) noexcept {
  std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
}

// Value-returning handler: wrap the deferred call in `UnexpectedDeducer` for
// implicit error conversion.
template <typename T, typename E, typename Fn, typename... Args>
  requires std::invocable<Fn&&, Args&&..., E> &&
           (!std::is_void_v<std::invoke_result_t<Fn &&, Args && ..., E>>)
constexpr auto ErrorReturn(expected<T, E>&& result,
                           Fn&& fn,
                           Args&&... args) noexcept {
  return UnexpectedDeducer{std::forward<Fn>(fn), std::forward<Args>(args)...,
                           std::move(result).error()};
}

template <typename T, typename Fn, typename... Args>
  requires std::invocable<Fn&&, Args&&...> &&
           (!std::is_void_v<std::invoke_result_t<Fn &&, Args && ...>>)
constexpr auto ErrorReturn(std::optional<T>&&,
                           Fn&& fn,
                           Args&&... args) noexcept {
  return UnexpectedDeducer{std::forward<Fn>(fn), std::forward<Args>(args)...};
}

// Single non-handler value: forward it so it converts as `return v;` would.
// Returning a copy instead would dangle in functions that return a reference or
// a view.
template <typename T, typename E, typename Arg>
  requires(!std::invocable<Arg &&, E> && !IsHandler<std::decay_t<Arg>>)
constexpr Arg&& ErrorReturn(expected<T, E>&&,
                            Arg&& arg LIFETIME_BOUND) noexcept {
  return std::forward<Arg>(arg);
}

template <typename T, typename Arg>
  requires(!std::invocable<Arg &&> && !IsHandler<std::decay_t<Arg>>)
constexpr Arg&& ErrorReturn(std::optional<T>&&,
                            Arg&& arg LIFETIME_BOUND) noexcept {
  return std::forward<Arg>(arg);
}

}  // namespace base::internal_expected_macros

#define BASE_INTERNAL_EXPECTED_PASS_ARGS(func, ...) func(__VA_ARGS__)

#define BASE_INTERNAL_EXPECTED_BODY(expected, rexpr, name, ...)           \
  auto expected = (rexpr);                                                \
  {                                                                       \
    static_assert(                                                        \
        base::internal::IsExpected<decltype(expected)> ||                 \
            base::is_instantiation<decltype(expected), std::optional>,    \
        #name                                                             \
        " should only be used with base::expected<> or std::optional<>"); \
  }                                                                       \
  if (!expected.has_value()) [[unlikely]] {                               \
    return base::internal_expected_macros::ErrorReturn(                   \
        std::move(expected) __VA_OPT__(, ) __VA_ARGS__);                  \
  }

#define BASE_INTERNAL_EXPECTED_RETURN_IF_ERROR(expected, rexpr, ...) \
  do {                                                               \
    BASE_INTERNAL_EXPECTED_BODY(expected, rexpr, RETURN_IF_ERROR,    \
                                __VA_ARGS__);                        \
  } while (false)

#define BASE_INTERNAL_EXPECTED_ASSIGN_OR_RETURN(lhs, expected, rexpr, ...)     \
  {                                                                            \
    constexpr auto lhs_v = std::string_view(#lhs);                             \
    static_assert(!(lhs_v.front() == '(' && lhs_v.back() == ')' &&             \
                    lhs_v.rfind('?') != std::string_view::npos),               \
                  "Identified possible ternary in `lhs`; avoid passing "       \
                  "parenthesized expressions containing '?' to the first "     \
                  "argument of ASSIGN_OR_RETURN()");                           \
  }                                                                            \
  BASE_INTERNAL_EXPECTED_BODY(expected, rexpr, ASSIGN_OR_RETURN, __VA_ARGS__); \
  BASE_REMOVE_PARENS(lhs) = std::move(expected).value();

#endif  // BASE_TYPES_EXPECTED_MACROS_H_
