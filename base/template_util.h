// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEMPLATE_UTIL_H_
#define BASE_TEMPLATE_UTIL_H_

#include <stddef.h>
#include <iosfwd>
#include <iterator>
#include <type_traits>
#include <utility>

#include "base/compiler_specific.h"
#include "build/build_config.h"

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ <= 7
#include <vector>
#endif

// Some versions of libstdc++ have partial support for type_traits, but misses
// a smaller subset while removing some of the older non-standard stuff. Assume
// that all versions below 5.0 fall in this category, along with one 5.0
// experimental release. Test for this by consulting compiler major version,
// the only reliable option available, so theoretically this could fail should
// you attempt to mix an earlier version of libstdc++ with >= GCC5. But
// that's unlikely to work out, especially as GCC5 changed ABI.
#define CR_GLIBCXX_5_0_0 20150123
#if (defined(__GNUC__) && __GNUC__ < 5) || \
    (defined(__GLIBCXX__) && __GLIBCXX__ == CR_GLIBCXX_5_0_0)
#define CR_USE_FALLBACKS_FOR_OLD_EXPERIMENTAL_GLIBCXX
#endif

// This hacks around using gcc with libc++ which has some incompatibilies.
// - is_trivially_* doesn't work: https://llvm.org/bugs/show_bug.cgi?id=27538
// TODO(danakj): Remove this when android builders are all using a newer version
// of gcc, or the android ndk is updated to a newer libc++ that works with older
// gcc versions.
#if !defined(__clang__) && defined(_LIBCPP_VERSION)
#define CR_USE_FALLBACKS_FOR_GCC_WITH_LIBCXX
#endif

namespace base {

template <class T> struct is_non_const_reference : std::false_type {};
template <class T> struct is_non_const_reference<T&> : std::true_type {};
template <class T> struct is_non_const_reference<const T&> : std::false_type {};

namespace internal {

// Implementation detail of base::void_t below.
template <typename...>
struct make_void {
  using type = void;
};

}  // namespace internal

// base::void_t is an implementation of std::void_t from C++17.
//
// We use |base::internal::make_void| as a helper struct to avoid a C++14
// defect:
//   http://en.cppreference.com/w/cpp/types/void_t
//   http://open-std.org/JTC1/SC22/WG21/docs/cwg_defects.html#1558
template <typename... Ts>
using void_t = typename ::base::internal::make_void<Ts...>::type;

namespace internal {

// Uses expression SFINAE to detect whether using operator<< would work.
template <typename T, typename = void>
struct SupportsOstreamOperator : std::false_type {};
template <typename T>
struct SupportsOstreamOperator<T,
                               decltype(void(std::declval<std::ostream&>()
                                             << std::declval<T>()))>
    : std::true_type {};

template <typename T, typename = void>
struct SupportsToString : std::false_type {};
template <typename T>
struct SupportsToString<T, decltype(void(std::declval<T>().ToString()))>
    : std::true_type {};

// Used to detech whether the given type is an iterator.  This is normally used
// with std::enable_if to provide disambiguation for functions that take
// templatzed iterators as input.
template <typename T, typename = void>
struct is_iterator : std::false_type {};

template <typename T>
struct is_iterator<T,
                   void_t<typename std::iterator_traits<T>::iterator_category>>
    : std::true_type {};

// Helper to express preferences in an overload set. If more than one overload
// are available for a given set of parameters the overload with the higher
// priority will be chosen.
template <size_t I>
struct priority_tag : priority_tag<I - 1> {};

template <>
struct priority_tag<0> {};

}  // namespace internal

// is_trivially_copyable is especially hard to get right.
// - Older versions of libstdc++ will fail to have it like they do for other
//   type traits. This has become a subset of the second point, but used to be
//   handled independently.
// - An experimental release of gcc includes most of type_traits but misses
//   is_trivially_copyable, so we still have to avoid using libstdc++ in this
//   case, which is covered by CR_USE_FALLBACKS_FOR_OLD_EXPERIMENTAL_GLIBCXX.
// - When compiling libc++ from before r239653, with a gcc compiler, the
//   std::is_trivially_copyable can fail. So we need to work around that by not
//   using the one in libc++ in this case. This is covered by the
//   CR_USE_FALLBACKS_FOR_GCC_WITH_LIBCXX define, and is discussed in
//   https://llvm.org/bugs/show_bug.cgi?id=27538#c1 where they point out that
//   in libc++'s commit r239653 this is fixed by libc++ checking for gcc 5.1.
// - In both of the above cases we are using the gcc compiler. When defining
//   this ourselves on compiler intrinsics, the __is_trivially_copyable()
//   intrinsic is not available on gcc before version 5.1 (see the discussion in
//   https://llvm.org/bugs/show_bug.cgi?id=27538#c1 again), so we must check for
//   that version.
// - When __is_trivially_copyable() is not available because we are on gcc older
//   than 5.1, we need to fall back to something, so we use __has_trivial_copy()
//   instead based on what was done one-off in bit_cast() previously.

// TODO(crbug.com/554293): Remove this when all platforms have this in the std
// namespace and it works with gcc as needed.
#if defined(CR_USE_FALLBACKS_FOR_OLD_EXPERIMENTAL_GLIBCXX) || \
    defined(CR_USE_FALLBACKS_FOR_GCC_WITH_LIBCXX)
template <typename T>
struct is_trivially_copyable {
// TODO(danakj): Remove this when android builders are all using a newer version
// of gcc, or the android ndk is updated to a newer libc++ that does this for
// us.
#if _GNUC_VER >= 501
  static constexpr bool value = __is_trivially_copyable(T);
#else
  static constexpr bool value =
      __has_trivial_copy(T) && __has_trivial_destructor(T);
#endif
};
#else
template <class T>
using is_trivially_copyable = std::is_trivially_copyable<T>;
#endif

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ <= 7
// Workaround for g++7 and earlier family.
// Due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80654, without this
// absl::optional<std::vector<T>> where T is non-copyable causes a compile
// error.  As we know it is not trivially copy constructible, explicitly declare
// so.
template <typename T>
struct is_trivially_copy_constructible
    : std::is_trivially_copy_constructible<T> {};

template <typename... T>
struct is_trivially_copy_constructible<std::vector<T...>> : std::false_type {};
#else
// Otherwise use std::is_trivially_copy_constructible as is.
template <typename T>
using is_trivially_copy_constructible = std::is_trivially_copy_constructible<T>;
#endif

// base::in_place_t is an implementation of std::in_place_t from
// C++17. A tag type used to request in-place construction in template vararg
// constructors.

// Specification:
// https://en.cppreference.com/w/cpp/utility/in_place
struct in_place_t {};
constexpr in_place_t in_place = {};

// base::in_place_type_t is an implementation of std::in_place_type_t from
// C++17. A tag type used for in-place construction when the type to construct
// needs to be specified, such as with base::unique_any, designed to be a
// drop-in replacement.

// Specification:
// http://en.cppreference.com/w/cpp/utility/in_place
template <typename T>
struct in_place_type_t {};

template <typename T>
struct is_in_place_type_t {
  static constexpr bool value = false;
};

template <typename... Ts>
struct is_in_place_type_t<in_place_type_t<Ts...>> {
  static constexpr bool value = true;
};

// C++14 implementation of C++17's std::bool_constant.
//
// Reference: https://en.cppreference.com/w/cpp/types/integral_constant
// Specification: https://wg21.link/meta.type.synop
template <bool B>
using bool_constant = std::integral_constant<bool, B>;

// C++14 implementation of C++17's std::conjunction.
//
// Reference: https://en.cppreference.com/w/cpp/types/conjunction
// Specification: https://wg21.link/meta.logical#1.itemdecl:1
template <typename...>
struct conjunction : std::true_type {};

template <typename B1>
struct conjunction<B1> : B1 {};

template <typename B1, typename... Bn>
struct conjunction<B1, Bn...>
    : std::conditional_t<static_cast<bool>(B1::value), conjunction<Bn...>, B1> {
};

// C++14 implementation of C++17's std::disjunction.
//
// Reference: https://en.cppreference.com/w/cpp/types/disjunction
// Specification: https://wg21.link/meta.logical#itemdecl:2
template <typename...>
struct disjunction : std::false_type {};

template <typename B1>
struct disjunction<B1> : B1 {};

template <typename B1, typename... Bn>
struct disjunction<B1, Bn...>
    : std::conditional_t<static_cast<bool>(B1::value), B1, disjunction<Bn...>> {
};

// C++14 implementation of C++17's std::negation.
//
// Reference: https://en.cppreference.com/w/cpp/types/negation
// Specification: https://wg21.link/meta.logical#itemdecl:3
template <typename B>
struct negation : bool_constant<!static_cast<bool>(B::value)> {};

// Implementation of C++17's invoke_result.
//
// This implementation adds references to `Functor` and `Args` to work around
// some quirks of std::result_of. See the #Notes section of [1] for details.
//
// References:
// [1] https://en.cppreference.com/w/cpp/types/result_of
// [2] https://wg21.link/meta.trans.other#lib:invoke_result
template <typename Functor, typename... Args>
using invoke_result = std::result_of<Functor && (Args && ...)>;

// Implementation of C++17's std::invoke_result_t.
//
// Reference: https://wg21.link/meta.type.synop#lib:invoke_result_t
template <typename Functor, typename... Args>
using invoke_result_t = typename invoke_result<Functor, Args...>::type;

namespace internal {

// Base case, `InvokeResult` does not have a nested type member. This means `F`
// could not be invoked with `Args...` and thus is not invocable.
template <typename InvokeResult, typename R, typename = void>
struct IsInvocableImpl : std::false_type {};

// Happy case, `InvokeResult` does have a nested type member. Now check whether
// `InvokeResult::type` is convertible to `R`. Short circuit in case
// `std::is_void<R>`.
template <typename InvokeResult, typename R>
struct IsInvocableImpl<InvokeResult, R, void_t<typename InvokeResult::type>>
    : disjunction<std::is_void<R>,
                  std::is_convertible<typename InvokeResult::type, R>> {};

}  // namespace internal

// Implementation of C++17's std::is_invocable_r.
//
// Returns whether `F` can be invoked with `Args...` and the result is
// convertible to `R`.
//
// Reference: https://wg21.link/meta.rel#lib:is_invocable_r
template <typename R, typename F, typename... Args>
struct is_invocable_r
    : internal::IsInvocableImpl<invoke_result<F, Args...>, R> {};

// Implementation of C++17's std::is_invocable.
//
// Returns whether `F` can be invoked with `Args...`.
//
// Reference: https://wg21.link/meta.rel#lib:is_invocable
template <typename F, typename... Args>
struct is_invocable : is_invocable_r<void, F, Args...> {};

namespace internal {

// The indirection with std::is_enum<T> is required, because instantiating
// std::underlying_type_t<T> when T is not an enum is UB prior to C++20.
template <typename T, bool = std::is_enum<T>::value>
struct IsScopedEnumImpl : std::false_type {};

template <typename T>
struct IsScopedEnumImpl<T, /*std::is_enum<T>::value=*/true>
    : negation<std::is_convertible<T, std::underlying_type_t<T>>> {};

}  // namespace internal

// Implementation of C++23's std::is_scoped_enum
//
// Reference: https://en.cppreference.com/w/cpp/types/is_scoped_enum
template <typename T>
struct is_scoped_enum : internal::IsScopedEnumImpl<T> {};

// Implementation of C++20's std::remove_cvref.
//
// References:
// - https://en.cppreference.com/w/cpp/types/remove_cvref
// - https://wg21.link/meta.trans.other#lib:remove_cvref
template <typename T>
struct remove_cvref {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
};

// Implementation of C++20's std::remove_cvref_t.
//
// References:
// - https://en.cppreference.com/w/cpp/types/remove_cvref
// - https://wg21.link/meta.type.synop#lib:remove_cvref_t
template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

// Implementation of C++20's std::is_constant_evaluated.
//
// References:
// - https://en.cppreference.com/w/cpp/types/is_constant_evaluated
// - https://wg21.link/meta.const.eval
constexpr bool is_constant_evaluated() noexcept {
#if HAS_BUILTIN(__builtin_is_constant_evaluated)
  return __builtin_is_constant_evaluated();
#else
  return false;
#endif
}

// Simplified implementation of C++20's std::iter_value_t.
// As opposed to std::iter_value_t, this implementation does not restrict
// the type of `Iter` and does not consider specializations of
// `indirectly_readable_traits`.
//
// Reference: https://wg21.link/readable.traits#2
template <typename Iter>
using iter_value_t =
    typename std::iterator_traits<remove_cvref_t<Iter>>::value_type;

// Simplified implementation of C++20's std::iter_reference_t.
// As opposed to std::iter_reference_t, this implementation does not restrict
// the type of `Iter`.
//
// Reference: https://wg21.link/iterator.synopsis#:~:text=iter_reference_t
template <typename Iter>
using iter_reference_t = decltype(*std::declval<Iter&>());

// Simplified implementation of C++20's std::indirect_result_t. As opposed to
// std::indirect_result_t, this implementation does not restrict the type of
// `Func` and `Iters`.
//
// Reference: https://wg21.link/iterator.synopsis#:~:text=indirect_result_t
template <typename Func, typename... Iters>
using indirect_result_t = invoke_result_t<Func, iter_reference_t<Iters>...>;

// Simplified implementation of C++20's std::projected. As opposed to
// std::projected, this implementation does not explicitly restrict the type of
// `Iter` and `Proj`, but rather does so implicitly by requiring
// `indirect_result_t<Proj, Iter>` is a valid type. This is required for SFINAE
// friendliness.
//
// Reference: https://wg21.link/projected
template <typename Iter,
          typename Proj,
          typename IndirectResultT = indirect_result_t<Proj, Iter>>
struct projected {
  using value_type = remove_cvref_t<IndirectResultT>;

  IndirectResultT operator*() const;  // not defined
};

}  // namespace base

#undef CR_USE_FALLBACKS_FOR_GCC_WITH_LIBCXX
#undef CR_USE_FALLBACKS_FOR_OLD_EXPERIMENTAL_GLIBCXX

#endif  // BASE_TEMPLATE_UTIL_H_
