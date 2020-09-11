// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RANGES_FUNCTIONAL_H_
#define BASE_RANGES_FUNCTIONAL_H_

#include <functional>
#include <type_traits>
#include <utility>

namespace base {

namespace internal {

// Helper struct and alias to deduce the class type from a member function
// pointer or member object pointer.
template <typename DecayedF>
struct member_pointer_class {};

template <typename ReturnT, typename ClassT>
struct member_pointer_class<ReturnT ClassT::*> {
  using type = ClassT;
};

template <typename DecayedF>
using member_pointer_class_t = typename member_pointer_class<DecayedF>::type;

// Utility struct to detect specializations of std::reference_wrapper.
template <typename T>
struct is_reference_wrapper : std::false_type {};

template <typename T>
struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type {};

// Small helpers used below in internal::invoke to make the SFINAE more concise.
template <typename F>
const bool& IsMemFunPtr =
    std::is_member_function_pointer<std::decay_t<F>>::value;

template <typename F>
const bool& IsMemObjPtr = std::is_member_object_pointer<std::decay_t<F>>::value;

template <typename F,
          typename T,
          typename MemPtrClass = member_pointer_class_t<std::decay_t<F>>>
const bool& IsMemPtrToBaseOf =
    std::is_base_of<MemPtrClass, std::decay_t<T>>::value;

template <typename T>
const bool& IsRefWrapper = is_reference_wrapper<std::decay_t<T>>::value;

template <bool B>
using EnableIf = std::enable_if_t<B, bool>;

// Invokes a member function pointer on a reference to an object of a suitable
// type. Covers bullet 1 of the INVOKE definition.
//
// Reference: https://wg21.link/func.require#1.1
template <typename F,
          typename T1,
          typename... Args,
          EnableIf<IsMemFunPtr<F> && IsMemPtrToBaseOf<F, T1>> = true>
constexpr decltype(auto) invoke(F&& f, T1&& t1, Args&&... args) {
  return (std::forward<T1>(t1).*f)(std::forward<Args>(args)...);
}

// Invokes a member function pointer on a std::reference_wrapper to an object of
// a suitable type. Covers bullet 2 of the INVOKE definition.
//
// Reference: https://wg21.link/func.require#1.2
template <typename F,
          typename T1,
          typename... Args,
          EnableIf<IsMemFunPtr<F> && IsRefWrapper<T1>> = true>
constexpr decltype(auto) invoke(F&& f, T1&& t1, Args&&... args) {
  return (t1.get().*f)(std::forward<Args>(args)...);
}

// Invokes a member function pointer on a pointer-like type to an object of a
// suitable type. Covers bullet 3 of the INVOKE definition.
//
// Reference: https://wg21.link/func.require#1.3
template <typename F,
          typename T1,
          typename... Args,
          EnableIf<IsMemFunPtr<F> && !IsMemPtrToBaseOf<F, T1> &&
                   !IsRefWrapper<T1>> = true>
constexpr decltype(auto) invoke(F&& f, T1&& t1, Args&&... args) {
  return ((*std::forward<T1>(t1)).*f)(std::forward<Args>(args)...);
}

// Invokes a member object pointer on a reference to an object of a suitable
// type. Covers bullet 4 of the INVOKE definition.
//
// Reference: https://wg21.link/func.require#1.4
template <typename F,
          typename T1,
          EnableIf<IsMemObjPtr<F> && IsMemPtrToBaseOf<F, T1>> = true>
constexpr decltype(auto) invoke(F&& f, T1&& t1) {
  return std::forward<T1>(t1).*f;
}

// Invokes a member object pointer on a std::reference_wrapper to an object of
// a suitable type. Covers bullet 5 of the INVOKE definition.
//
// Reference: https://wg21.link/func.require#1.5
template <typename F,
          typename T1,
          EnableIf<IsMemObjPtr<F> && IsRefWrapper<T1>> = true>
constexpr decltype(auto) invoke(F&& f, T1&& t1) {
  return t1.get().*f;
}

// Invokes a member object pointer on a pointer-like type to an object of a
// suitable type. Covers bullet 6 of the INVOKE definition.
//
// Reference: https://wg21.link/func.require#1.6
template <typename F,
          typename T1,
          EnableIf<IsMemObjPtr<F> && !IsMemPtrToBaseOf<F, T1> &&
                   !IsRefWrapper<T1>> = true>
constexpr decltype(auto) invoke(F&& f, T1&& t1) {
  return (*std::forward<T1>(t1)).*f;
}

// Invokes a regular function or function object. Covers bullet 7 of the INVOKE
// definition.
//
// Reference: https://wg21.link/func.require#1.7
template <typename F, typename... Args>
constexpr decltype(auto) invoke(F&& f, Args&&... args) {
  return std::forward<F>(f)(std::forward<Args>(args)...);
}

}  // namespace internal

// Implementation of C++17's std::invoke. This is not based on implementation
// referenced in original std::invoke proposal, but rather a manual
// implementation, so that it can be constexpr.
//
// References:
// - https://wg21.link/n4169#implementability
// - https://en.cppreference.com/w/cpp/utility/functional/invoke
// - https://wg21.link/func.invoke
template <typename F, typename... Args>
constexpr decltype(auto) invoke(F&& f, Args&&... args) {
  return internal::invoke(std::forward<F>(f), std::forward<Args>(args)...);
}

// Implementation of C++17's std::invoke_result_t.
//
// This implementation adds references to `Functor` and `Args` to work around
// some quirks of std::result_of_t. See the #Notes section of [1] for details.
//
// References:
// [1] https://en.cppreference.com/w/cpp/types/result_of
// [2] https://wg21.link/meta.type.synop#lib:invoke_result_t
template <typename Functor, typename... Args>
using invoke_result_t = std::result_of_t<Functor && (Args && ...)>;

// Implementation of C++20's std::identity.
//
// Reference:
// - https://en.cppreference.com/w/cpp/utility/functional/identity
// - https://wg21.link/func.identity
struct identity {
  template <typename T>
  constexpr T&& operator()(T&& t) const noexcept {
    return std::forward<T>(t);
  }

  using is_transparent = void;
};

// Simplified implementations of C++20's std::ranges comparison function
// objects. As opposed to the std::ranges implementation, these versions do not
// constrain the passed-in types.
//
// Reference: https://wg21.link/range.cmp
namespace ranges {
using equal_to = std::equal_to<>;
using not_equal_to = std::not_equal_to<>;
using greater = std::greater<>;
using less = std::less<>;
using greater_equal = std::greater_equal<>;
using less_equal = std::less_equal<>;
}  // namespace ranges

}  // namespace base

#endif  // BASE_RANGES_FUNCTIONAL_H_
