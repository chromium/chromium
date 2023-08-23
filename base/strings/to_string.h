// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_TO_STRING_H_
#define BASE_STRINGS_TO_STRING_H_

#include <ios>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/template_util.h"
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
template <typename T, typename = void>
constexpr bool IsIomanip = false;
template <typename T>
constexpr bool
    IsIomanip<T&(T&), std::enable_if_t<std::is_base_of_v<std::ios_base, T>>> =
        true;

// Function pointers implicitly convert to `bool`, so use this to avoid printing
// function pointers as 1 or 0.
template <typename T, typename = void>
constexpr bool WillBeIncorrectlyStreamedAsBool = false;
template <typename T>
constexpr bool WillBeIncorrectlyStreamedAsBool<
    T,
    std::enable_if_t<std::is_function_v<std::remove_pointer_t<T>> &&
                     !IsIomanip<std::remove_pointer_t<T>>>> = true;

// Fallback case when there is no better representation.
template <typename T, typename = void>
struct ToStringHelper {
  static void Stringify(const T& v, std::ostringstream& ss) {
    ss << "[" << sizeof(v) << "-byte object at 0x" << std::addressof(v) << "]";
  }
};

// Most streamables.
template <typename T>
struct ToStringHelper<T,
                      std::enable_if_t<SupportsOstreamOperator<const T&> &&
                                       !WillBeIncorrectlyStreamedAsBool<T>>> {
  static void Stringify(const T& v, std::ostringstream& ss) { ss << v; }
};

// Functions and function pointers.
template <typename T>
struct ToStringHelper<T,
                      std::enable_if_t<SupportsOstreamOperator<const T&> &&
                                       WillBeIncorrectlyStreamedAsBool<T>>> {
  static void Stringify(const T& v, std::ostringstream& ss) {
    ToStringHelper<const void*>::Stringify(reinterpret_cast<const void*>(v),
                                           ss);
  }
};

// Non-streamables that have a `ToString` member.
template <typename T>
struct ToStringHelper<T,
                      std::enable_if_t<!SupportsOstreamOperator<const T&> &&
                                       SupportsToString<const T&>>> {
  static void Stringify(const T& v, std::ostringstream& ss) {
    // .ToString() may not return a std::string, e.g. blink::WTF::String.
    ToStringHelper<decltype(v.ToString())>::Stringify(v.ToString(), ss);
  }
};

// Non-streamable enums (i.e. scoped enums where no `operator<<` overload was
// declared).
template <typename T>
struct ToStringHelper<
    T,
    std::enable_if_t<!SupportsOstreamOperator<const T&> && std::is_enum_v<T>>> {
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
// methods if they exist.
template <typename... Ts>
std::string ToString(const Ts&... values) {
  std::ostringstream ss;
  (..., internal::ToStringHelper<remove_cvref_t<decltype(values)>>::Stringify(
            values, ss));
  return ss.str();
}

}  // namespace base

#endif  // BASE_STRINGS_TO_STRING_H_
