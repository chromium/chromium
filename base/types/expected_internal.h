// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_EXPECTED_INTERNAL_H_
#define BASE_TYPES_EXPECTED_INTERNAL_H_

// IWYU pragma: private, include "base/types/expected.h"
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/template_util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

// This header defines type traits and aliases used for the implementation of
// base::expected.
namespace base {

template <typename T, bool = std::is_void_v<T>>
class ok;

template <typename E>
class unexpected;

struct unexpect_t {
  explicit unexpect_t() = default;
};

// in-place construction of unexpected values
inline constexpr unexpect_t unexpect{};

template <typename T, typename E, bool = std::is_void_v<T>>
class expected;

namespace internal {

template <typename T>
struct IsOk : std::false_type {};

template <typename T>
struct IsOk<ok<T>> : std::true_type {};

template <typename T>
struct IsUnexpected : std::false_type {};

template <typename E>
struct IsUnexpected<unexpected<E>> : std::true_type {};

template <typename T>
struct IsExpected : std::false_type {};

template <typename T, typename E>
struct IsExpected<expected<T, E>> : std::true_type {};

template <typename T, typename U>
struct IsConstructibleOrConvertible
    : std::disjunction<std::is_constructible<T, U>, std::is_convertible<U, T>> {
};

template <typename T, typename U>
struct IsAnyConstructibleOrConvertible
    : std::disjunction<IsConstructibleOrConvertible<T, U&>,
                       IsConstructibleOrConvertible<T, U&&>,
                       IsConstructibleOrConvertible<T, const U&>,
                       IsConstructibleOrConvertible<T, const U&&>> {};

// Checks whether a given expected<U, G> can be converted into another
// expected<T, E>. Used inside expected's conversion constructors. UF and GF are
// the forwarded versions of U and G, e.g. UF is const U& for the converting
// copy constructor and U for the converting move constructor. Similarly for GF.
// ExUG is used for convenience, and not expected to be passed explicitly.
// See https://eel.is/c++draft/expected#lib:expected,constructor___
template <typename T,
          typename E,
          typename UF,
          typename GF,
          typename ExUG = expected<remove_cvref_t<UF>, remove_cvref_t<GF>>>
struct IsValidConversion
    : std::conjunction<
          std::is_constructible<T, UF>,
          std::is_constructible<E, GF>,
          std::negation<IsAnyConstructibleOrConvertible<T, ExUG>>,
          std::negation<IsAnyConstructibleOrConvertible<unexpected<E>, ExUG>>> {
};

// Checks whether a given expected<U, G> can be converted into another
// expected<T, E> when T is a void type. Used inside expected<void>'s conversion
// constructors. GF is the forwarded versions of G, e.g. GF is const G& for the
// converting copy constructor and G for the converting move constructor. ExUG
// is used for convenience, and not expected to be passed explicitly. See
// https://eel.is/c++draft/expected#lib:expected%3cvoid%3e,constructor___
template <typename E,
          typename U,
          typename GF,
          typename ExUG = expected<U, remove_cvref_t<GF>>>
struct IsValidVoidConversion
    : std::conjunction<
          std::is_void<U>,
          std::is_constructible<E, GF>,
          std::negation<IsAnyConstructibleOrConvertible<unexpected<E>, ExUG>>> {
};

// Checks whether expected<T, E> can be constructed from a value of type U.
template <typename T, typename E, typename U>
struct IsValidValueConstruction
    : std::conjunction<
          std::is_constructible<T, U>,
          std::negation<std::is_same<remove_cvref_t<U>, absl::in_place_t>>,
          std::negation<std::is_same<remove_cvref_t<U>, expected<T, E>>>,
          std::negation<IsOk<remove_cvref_t<U>>>,
          std::negation<IsUnexpected<remove_cvref_t<U>>>> {};

template <typename T, typename E, typename UF, typename GF>
struct AreValueAndErrorConvertible
    : std::conjunction<std::is_convertible<UF, T>, std::is_convertible<GF, E>> {
};

template <typename T>
using EnableIfDefaultConstruction =
    std::enable_if_t<std::is_default_constructible_v<T>, int>;

template <typename T, typename E, typename UF, typename GF>
using EnableIfExplicitConversion = std::enable_if_t<
    std::conjunction_v<
        IsValidConversion<T, E, UF, GF>,
        std::negation<AreValueAndErrorConvertible<T, E, UF, GF>>>,
    int>;

template <typename T, typename E, typename UF, typename GF>
using EnableIfImplicitConversion = std::enable_if_t<
    std::conjunction_v<IsValidConversion<T, E, UF, GF>,
                       AreValueAndErrorConvertible<T, E, UF, GF>>,
    int>;

template <typename E, typename U, typename GF>
using EnableIfExplicitVoidConversion = std::enable_if_t<
    std::conjunction_v<IsValidVoidConversion<E, U, GF>,
                       std::negation<std::is_convertible<GF, E>>>,
    int>;

template <typename E, typename U, typename GF>
using EnableIfImplicitVoidConversion =
    std::enable_if_t<std::conjunction_v<IsValidVoidConversion<E, U, GF>,
                                        std::is_convertible<GF, E>>,
                     int>;

template <typename T, typename U>
using EnableIfOkValueConstruction = std::enable_if_t<
    std::conjunction_v<
        std::negation<std::is_same<remove_cvref_t<U>, ok<T>>>,
        std::negation<std::is_same<remove_cvref_t<U>, absl::in_place_t>>,
        std::is_constructible<T, U>>,
    int>;

template <typename T, typename U>
using EnableIfUnexpectedValueConstruction = std::enable_if_t<
    std::conjunction_v<
        std::negation<std::is_same<remove_cvref_t<U>, unexpected<T>>>,
        std::negation<std::is_same<remove_cvref_t<U>, absl::in_place_t>>,
        std::is_constructible<T, U>>,
    int>;

template <typename T, typename E, typename U>
using EnableIfExplicitValueConstruction = std::enable_if_t<
    std::conjunction_v<
        IsValidValueConstruction<T, E, U>,
        std::disjunction<std::negation<std::is_convertible<U, T>>,
                         std::is_convertible<U, E>>>,
    int>;

template <typename T, typename E, typename U>
using EnableIfImplicitValueConstruction = std::enable_if_t<
    std::conjunction_v<
        IsValidValueConstruction<T, E, U>,
        std::conjunction<std::is_convertible<U, T>,
                         std::negation<std::is_convertible<U, E>>>>,
    int>;

template <typename T, typename U>
using EnableIfExplicitConstruction = std::enable_if_t<
    std::conjunction_v<std::is_constructible<T, U>,
                       std::negation<std::is_convertible<U, T>>>,
    int>;

template <typename T, typename U>
using EnableIfImplicitConstruction = std::enable_if_t<
    std::conjunction_v<std::is_constructible<T, U>, std::is_convertible<U, T>>,
    int>;

template <typename T, typename E, typename U>
using EnableIfValueAssignment = std::enable_if_t<
    std::conjunction_v<
        std::negation<std::is_same<expected<T, E>, remove_cvref_t<U>>>,
        std::negation<IsOk<remove_cvref_t<U>>>,
        std::negation<IsUnexpected<remove_cvref_t<U>>>,
        std::is_constructible<T, U>,
        std::is_assignable<T&, U>>,
    int>;

template <typename T>
using EnableIfCopyConstructible =
    std::enable_if_t<std::is_copy_constructible_v<T>, int>;

template <typename T>
using EnableIfMoveConstructible =
    std::enable_if_t<std::is_move_constructible_v<T>, int>;

template <typename T>
using EnableIfNotVoid = std::enable_if_t<std::negation_v<std::is_void<T>>, int>;

template <typename T, typename E>
class ExpectedImpl {
 public:
  static constexpr size_t kValIdx = 1;
  static constexpr size_t kErrIdx = 2;
  static constexpr absl::in_place_index_t<1> kValTag{};
  static constexpr absl::in_place_index_t<2> kErrTag{};

  template <typename U, typename G>
  friend class ExpectedImpl;

  template <typename LazyT = T, EnableIfDefaultConstruction<LazyT> = 0>
  constexpr ExpectedImpl() noexcept : data_(kValTag) {}
  constexpr ExpectedImpl(const ExpectedImpl& rhs) noexcept : data_(rhs.data_) {
    CHECK(!rhs.is_moved_from());
  }
  constexpr ExpectedImpl(ExpectedImpl&& rhs) noexcept
      : data_(std::move(rhs.data_)) {
    CHECK(!rhs.is_moved_from());
    rhs.set_is_moved_from();
  }

  template <typename U, typename G>
  constexpr explicit ExpectedImpl(const ExpectedImpl<U, G>& rhs) noexcept {
    if (rhs.has_value()) {
      emplace_value(rhs.value());
    } else {
      emplace_error(rhs.error());
    }
  }

  template <typename U, typename G>
  constexpr explicit ExpectedImpl(ExpectedImpl<U, G>&& rhs) noexcept {
    if (rhs.has_value()) {
      emplace_value(std::move(rhs.value()));
    } else {
      emplace_error(std::move(rhs.error()));
    }
    rhs.set_is_moved_from();
  }

  template <typename... Args>
  constexpr explicit ExpectedImpl(decltype(kValTag), Args&&... args) noexcept
      : data_(kValTag, std::forward<Args>(args)...) {}

  template <typename U, typename... Args>
  constexpr explicit ExpectedImpl(decltype(kValTag),
                                  std::initializer_list<U> il,
                                  Args&&... args) noexcept
      : data_(kValTag, il, std::forward<Args>(args)...) {}

  template <typename... Args>
  constexpr explicit ExpectedImpl(decltype(kErrTag), Args&&... args) noexcept
      : data_(kErrTag, std::forward<Args>(args)...) {}

  template <typename U, typename... Args>
  constexpr explicit ExpectedImpl(decltype(kErrTag),
                                  std::initializer_list<U> il,
                                  Args&&... args) noexcept
      : data_(kErrTag, il, std::forward<Args>(args)...) {}

  constexpr ExpectedImpl& operator=(const ExpectedImpl& rhs) noexcept {
    CHECK(!rhs.is_moved_from());
    data_ = rhs.data_;
    return *this;
  }

  constexpr ExpectedImpl& operator=(ExpectedImpl&& rhs) noexcept {
    CHECK(!rhs.is_moved_from());
    data_ = std::move(rhs.data_);
    rhs.set_is_moved_from();
    return *this;
  }

  template <typename... Args>
  constexpr T& emplace_value(Args&&... args) noexcept {
    return data_.template emplace<kValIdx>(std::forward<Args>(args)...);
  }

  template <typename U, typename... Args>
  constexpr T& emplace_value(std::initializer_list<U> il,
                             Args&&... args) noexcept {
    return data_.template emplace<kValIdx>(il, std::forward<Args>(args)...);
  }

  template <typename... Args>
  constexpr E& emplace_error(Args&&... args) noexcept {
    return data_.template emplace<kErrIdx>(std::forward<Args>(args)...);
  }

  template <typename U, typename... Args>
  constexpr E& emplace_error(std::initializer_list<U> il,
                             Args&&... args) noexcept {
    return data_.template emplace<kErrIdx>(il, std::forward<Args>(args)...);
  }

  void swap(ExpectedImpl& rhs) noexcept {
    CHECK(!is_moved_from());
    CHECK(!rhs.is_moved_from());
    data_.swap(rhs.data_);
  }

  constexpr bool has_value() const noexcept {
    CHECK(!is_moved_from());
    return data_.index() == kValIdx;
  }

  // Note: No `CHECK()` here and below, since absl::get already checks that
  // the passed in index is active.
  constexpr T& value() noexcept { return absl::get<kValIdx>(data_); }
  constexpr const T& value() const noexcept {
    return absl::get<kValIdx>(data_);
  }

  constexpr E& error() noexcept { return absl::get<kErrIdx>(data_); }
  constexpr const E& error() const noexcept {
    return absl::get<kErrIdx>(data_);
  }

 private:
  static constexpr size_t kNulIdx = 0;
  static_assert(kNulIdx != kValIdx);
  static_assert(kNulIdx != kErrIdx);

  constexpr bool is_moved_from() const noexcept {
    return data_.index() == kNulIdx;
  }

  constexpr void set_is_moved_from() noexcept {
    data_.template emplace<kNulIdx>();
  }

  absl::variant<absl::monostate, T, E> data_;
};

template <typename Exp, typename F>
constexpr auto AndThen(Exp&& exp, F&& f) noexcept {
  using T = remove_cvref_t<decltype(exp.value())>;
  using E = remove_cvref_t<decltype(exp.error())>;

  auto invoke_f = [&]() -> decltype(auto) {
    if constexpr (!std::is_void_v<T>) {
      return std::invoke(std::forward<F>(f), std::forward<Exp>(exp).value());
    } else {
      return std::invoke(std::forward<F>(f));
    }
  };

  using U = remove_cvref_t<decltype(invoke_f())>;
  static_assert(internal::IsExpected<U>::value,
                "expected<T, E>::and_then: Result of f() must be a "
                "specialization of expected");
  static_assert(
      std::is_same_v<typename U::error_type, E>,
      "expected<T, E>::and_then: Result of f() must have E as error_type");

  return exp.has_value() ? invoke_f()
                         : U(unexpect, std::forward<Exp>(exp).error());
}

template <typename Exp, typename F>
constexpr auto OrElse(Exp&& exp, F&& f) noexcept {
  using T = remove_cvref_t<decltype(exp.value())>;
  using G = remove_cvref_t<
      std::invoke_result_t<F, decltype(std::forward<Exp>(exp).error())>>;

  static_assert(internal::IsExpected<G>::value,
                "expected<T, E>::or_else: Result of f() must be a "
                "specialization of expected");
  static_assert(
      std::is_same_v<typename G::value_type, T>,
      "expected<T, E>::or_else: Result of f() must have T as value_type");

  if (!exp.has_value()) {
    return std::invoke(std::forward<F>(f), std::forward<Exp>(exp).error());
  }

  if constexpr (!std::is_void_v<T>) {
    return G(absl::in_place, std::forward<Exp>(exp).value());
  } else {
    return G();
  }
}

template <typename Exp, typename F>
constexpr auto Transform(Exp&& exp, F&& f) noexcept {
  using T = remove_cvref_t<decltype(exp.value())>;
  using E = remove_cvref_t<decltype(exp.error())>;

  auto invoke_f = [&]() -> decltype(auto) {
    if constexpr (!std::is_void_v<T>) {
      return std::invoke(std::forward<F>(f), std::forward<Exp>(exp).value());
    } else {
      return std::invoke(std::forward<F>(f));
    }
  };

  using U = std::remove_cv_t<decltype(invoke_f())>;
  if constexpr (!std::is_void_v<U>) {
    static_assert(!std::is_array_v<U>,
                  "expected<T, E>::transform: Result of f() should "
                  "not be an Array");
    static_assert(!std::is_same_v<U, absl::in_place_t>,
                  "expected<T, E>::transform: Result of f() should "
                  "not be absl::in_place_t");
    static_assert(!std::is_same_v<U, unexpect_t>,
                  "expected<T, E>::transform: Result of f() should "
                  "not be unexpect_t");
    static_assert(!internal::IsOk<U>::value,
                  "expected<T, E>::transform: Result of f() should "
                  "not be a specialization of ok");
    static_assert(!internal::IsUnexpected<U>::value,
                  "expected<T, E>::transform: Result of f() should "
                  "not be a specialization of unexpected");
    static_assert(std::is_object_v<U>,
                  "expected<T, E>::transform: Result of f() should be "
                  "an object type");
  }

  if (!exp.has_value()) {
    return expected<U, E>(unexpect, std::forward<Exp>(exp).error());
  }

  if constexpr (!std::is_void_v<U>) {
    return expected<U, E>(absl::in_place, invoke_f());
  } else {
    invoke_f();
    return expected<U, E>();
  }
}

template <typename Exp, typename F>
constexpr auto TransformError(Exp&& exp, F&& f) noexcept {
  using T = remove_cvref_t<decltype(exp.value())>;
  using G = std::remove_cv_t<
      std::invoke_result_t<F, decltype(std::forward<Exp>(exp).error())>>;

  static_assert(
      !std::is_array_v<G>,
      "expected<T, E>::transform_error: Result of f() should not be an Array");
  static_assert(!std::is_same_v<G, absl::in_place_t>,
                "expected<T, E>::transform_error: Result of f() should not be "
                "absl::in_place_t");
  static_assert(!std::is_same_v<G, unexpect_t>,
                "expected<T, E>::transform_error: Result of f() should not be "
                "unexpect_t");
  static_assert(!internal::IsOk<G>::value,
                "expected<T, E>::transform_error: Result of f() should not be "
                "a specialization of ok");
  static_assert(!internal::IsUnexpected<G>::value,
                "expected<T, E>::transform_error: Result of f() should not be "
                "a specialization of unexpected");
  static_assert(std::is_object_v<G>,
                "expected<T, E>::transform_error: Result of f() should be an "
                "object type");

  if (!exp.has_value()) {
    return expected<T, G>(
        unexpect,
        std::invoke(std::forward<F>(f), std::forward<Exp>(exp).error()));
  }

  if constexpr (std::is_void_v<T>) {
    return expected<T, G>();
  } else {
    return expected<T, G>(absl::in_place, std::forward<Exp>(exp).value());
  }
}

}  // namespace internal

}  // namespace base

#endif  // BASE_TYPES_EXPECTED_INTERNAL_H_
