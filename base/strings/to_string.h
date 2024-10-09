// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_TO_STRING_H_
#define BASE_STRINGS_TO_STRING_H_

#include <concepts>
#include <ios>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/types/supports_ostream_operator.h"

namespace base {

template <typename... Ts>
std::string ToString(const Ts&... values);

namespace internal {

template <typename T>
concept SupportsToString = requires(const T& t) { t.ToString(); };

// I/O manipulators are function pointers, but should be sent directly to the
// `ostream` instead of being cast to `const void*` like other function
// pointers.
template <typename T>
constexpr bool IsIomanip = false;
template <typename T>
  requires(std::derived_from<T, std::ios_base>)
constexpr bool IsIomanip<T&(T&)> = true;

// Function pointers implicitly convert to `bool`, so use this to avoid printing
// function pointers as 1 or 0.
template <typename T>
concept WillBeIncorrectlyStreamedAsBool =
    std::is_function_v<std::remove_pointer_t<T>> &&
    !IsIomanip<std::remove_pointer_t<T>>;

// Fallback case when there is no better representation.
template <typename T>
struct ToStringHelper {
  static void Stringify(const T& v, std::ostringstream& ss) {
    // We cast to `void*` to avoid converting a char-like type to char-like*
    // which operator<< treats as a string but does not support for multi-byte
    // char-like types.
    ss << "[" << sizeof(v) << "-byte object at 0x"
       << static_cast<const void*>(std::addressof(v)) << "]";
  }
};

// Most streamables.
template <typename T>
  requires(SupportsOstreamOperator<const T&> &&
           !WillBeIncorrectlyStreamedAsBool<T>)
struct ToStringHelper<T> {
  static void Stringify(const T& v, std::ostringstream& ss) { ss << v; }
};

// Functions and function pointers.
template <typename T>
  requires(SupportsOstreamOperator<const T&> &&
           WillBeIncorrectlyStreamedAsBool<T>)
struct ToStringHelper<T> {
  static void Stringify(const T& v, std::ostringstream& ss) {
    ToStringHelper<const void*>::Stringify(reinterpret_cast<const void*>(v),
                                           ss);
  }
};

// Integral types that can't stream, like char16_t.
template <typename T>
  requires(!SupportsOstreamOperator<const T&> && std::is_integral_v<T>)
struct ToStringHelper<T> {
  static void Stringify(const T& v, std::ostringstream& ss) {
    if constexpr (std::is_signed_v<T>) {
      static_assert(sizeof(T) <= 8);
      ss << static_cast<int64_t>(v);
    } else {
      static_assert(sizeof(T) <= 8);
      ss << static_cast<uint64_t>(v);
    }
  }
};

// Non-streamables that have a `ToString` member.
template <typename T>
  requires(!SupportsOstreamOperator<const T&> && SupportsToString<const T&>)
struct ToStringHelper<T> {
  static void Stringify(const T& v, std::ostringstream& ss) {
    // .ToString() may not return a std::string, e.g. blink::WTF::String.
    ToStringHelper<decltype(v.ToString())>::Stringify(v.ToString(), ss);
  }
};

// Non-streamable enums (i.e. scoped enums where no `operator<<` overload was
// declared).
template <typename T>
  requires(!SupportsOstreamOperator<const T&> && std::is_enum_v<T>)
struct ToStringHelper<T> {
  static void Stringify(const T& v, std::ostringstream& ss) {
    using UT = typename std::underlying_type_t<T>;
    ToStringHelper<UT>::Stringify(static_cast<UT>(v), ss);
  }
};

// Tuples. Will recursively apply `ToString()` to each value in the tuple.
template <typename... T>
struct ToStringHelper<std::tuple<T...>> {
  template <size_t... I>
  static void StringifyHelper(const std::tuple<T...>& values,
                              std::index_sequence<I...>,
                              std::ostringstream& ss) {
    ss << "<";
    (..., (ss << (I == 0 ? "" : ", "), ss << ToString(std::get<I>(values))));
    ss << ">";
  }

  static void Stringify(const std::tuple<T...>& v, std::ostringstream& ss) {
    StringifyHelper(v, std::make_index_sequence<sizeof...(T)>(), ss);
  }
};

}  // namespace internal

// Converts any type to a string, preferring defined operator<<() or ToString()
// methods if they exist. If multiple `values` are given, returns the
// concatenation of the result of applying `ToString` to each value.
template <typename... Ts>
std::string ToString(const Ts&... values) {
  std::ostringstream ss;
  (...,
   internal::ToStringHelper<std::remove_cvref_t<decltype(values)>>::Stringify(
       values, ss));
  return ss.str();
}

}  // namespace base

#endif  // BASE_STRINGS_TO_STRING_H_
