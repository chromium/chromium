// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_EXPECTED_INTERNAL_H_
#define BASE_TYPES_EXPECTED_INTERNAL_H_

// IWYU pragma: private, include "base/types/expected.h"
#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
// TODO(dcheng): Remove this.
#include <variant>

#include "base/check.h"
#include "base/notreached.h"

// This header defines type traits and aliases used for the implementation of
// base::expected.
namespace base {

template <typename T>
class ok;

template <typename E>
class unexpected;

struct unexpect_t {
  unexpect_t() = default;
};

// in-place construction of unexpected values
inline constexpr unexpect_t unexpect{};

template <typename T, typename E>
class expected;

namespace internal {

template <typename T>
inline constexpr bool UnderlyingIsOk = false;
template <typename T>
inline constexpr bool UnderlyingIsOk<ok<T>> = true;
template <typename T>
inline constexpr bool IsOk = UnderlyingIsOk<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool UnderlyingIsUnexpected = false;
template <typename E>
inline constexpr bool UnderlyingIsUnexpected<unexpected<E>> = true;
template <typename T>
inline constexpr bool IsUnexpected =
    UnderlyingIsUnexpected<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool UnderlyingIsExpected = false;
template <typename T, typename E>
inline constexpr bool UnderlyingIsExpected<expected<T, E>> = true;
template <typename T>
inline constexpr bool IsExpected = UnderlyingIsExpected<std::remove_cvref_t<T>>;

template <typename T, typename U>
inline constexpr bool IsConstructibleOrConvertible =
    std::is_constructible_v<T, U> || std::is_convertible_v<U, T>;

template <typename T, typename U>
inline constexpr bool IsAnyConstructibleOrConvertible =
    IsConstructibleOrConvertible<T, U&> ||
    IsConstructibleOrConvertible<T, U&&> ||
    IsConstructibleOrConvertible<T, const U&> ||
    IsConstructibleOrConvertible<T, const U&&>;

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
          typename ExUG =
              expected<std::remove_cvref_t<UF>, std::remove_cvref_t<GF>>>
inline constexpr bool IsValidConversion =
    std::is_constructible_v<T, UF> && std::is_constructible_v<E, GF> &&
    !IsAnyConstructibleOrConvertible<T, ExUG> &&
    !IsAnyConstructibleOrConvertible<unexpected<E>, ExUG>;

// Checks whether a given expected<U, G> can be converted into another
// expected<T, E> when T is a void type. Used inside expected<void>'s conversion
// constructors. GF is the forwarded versions of G, e.g. GF is const G& for the
// converting copy constructor and G for the converting move constructor. ExUG
// is used for convenience, and not expected to be passed explicitly. See
// https://eel.is/c++draft/expected#lib:expected%3cvoid%3e,constructor___
template <typename E,
          typename U,
          typename GF,
          typename ExUG = expected<U, std::remove_cvref_t<GF>>>
inline constexpr bool IsValidVoidConversion =
    std::is_void_v<U> && std::is_constructible_v<E, GF> &&
    !IsAnyConstructibleOrConvertible<unexpected<E>, ExUG>;

// Checks whether expected<T, E> can be constructed from a value of type U.
template <typename T, typename E, typename U>
inline constexpr bool IsValidValueConstruction =
    std::is_constructible_v<T, U> &&
    !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> &&
    !std::is_same_v<std::remove_cvref_t<U>, expected<T, E>> && !IsOk<U> &&
    !IsUnexpected<U>;

template <typename T, typename U>
inline constexpr bool IsOkValueConstruction =
    !std::is_same_v<std::remove_cvref_t<U>, ok<T>> &&
    !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> &&
    std::is_constructible_v<T, U>;

template <typename T, typename U>
inline constexpr bool IsUnexpectedValueConstruction =
    !std::is_same_v<std::remove_cvref_t<U>, unexpected<T>> &&
    !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> &&
    std::is_constructible_v<T, U>;

template <typename T, typename E, typename U>
inline constexpr bool IsValueAssignment =
    !std::is_same_v<expected<T, E>, std::remove_cvref_t<U>> && !IsOk<U> &&
    !IsUnexpected<U> && std::is_constructible_v<T, U> &&
    std::is_assignable_v<T&, U>;

class ExpectedBase {
 protected:
  enum class State : uint8_t {
    kValue = 0,
    kError = 1,
    // A moved-from value and a moved-from error are distinct, since the
    // destructor still needs to know which union field to destroy.
    kMovedFromValue = 2,
    kMovedFromError = 3,
  };
};

template <typename T, typename E>
class ExpectedImpl : public ExpectedBase {
 public:
  static constexpr std::in_place_index_t<0> kValTag{};
  static constexpr std::in_place_index_t<1> kErrTag{};

  template <typename U, typename G>
  friend class ExpectedImpl;

  constexpr ExpectedImpl() noexcept
    requires(std::default_initializable<T>)
      : state_(State::kValue) {}

  constexpr ExpectedImpl(const ExpectedImpl& rhs) noexcept
      : state_(rhs.state_), storage_(CreateStorageByCopy(rhs)) {}
  constexpr ExpectedImpl(ExpectedImpl&& rhs) noexcept
      : state_(rhs.state_), storage_(CreateStorageByMove(std::move(rhs))) {}

  template <typename U, typename G>
  constexpr explicit ExpectedImpl(const ExpectedImpl<U, G>& rhs) noexcept
      : state_(rhs.state_), storage_(CreateStorageByCopy(rhs)) {}

  template <typename U, typename G>
  constexpr explicit ExpectedImpl(ExpectedImpl<U, G>&& rhs) noexcept
      : state_(rhs.state_), storage_(CreateStorageByMove(std::move(rhs))) {}

  template <typename... Args>
  constexpr explicit ExpectedImpl(decltype(kValTag), Args&&... args) noexcept
      : state_(State::kValue), storage_(kValTag, std::forward<Args>(args)...) {}

  template <typename U, typename... Args>
  constexpr ExpectedImpl(decltype(kValTag),
                         std::initializer_list<U> il,
                         Args&&... args) noexcept
      : state_(State::kValue),
        storage_(kValTag, il, std::forward<Args>(args)...) {}

  template <typename... Args>
  constexpr explicit ExpectedImpl(decltype(kErrTag), Args&&... args) noexcept
      : state_(State::kError), storage_(kErrTag, std::forward<Args>(args)...) {}

  template <typename U, typename... Args>
  constexpr ExpectedImpl(decltype(kErrTag),
                         std::initializer_list<U> il,
                         Args&&... args) noexcept
      : state_(State::kError),
        storage_(kErrTag, il, std::forward<Args>(args)...) {}

  constexpr ~ExpectedImpl()
    requires(!std::is_trivially_destructible_v<T> ||
             !std::is_trivially_destructible_v<E>)
  {
    DestroyStorage();
  }
  constexpr ~ExpectedImpl()
    requires(std::is_trivially_destructible_v<T> &&
             std::is_trivially_destructible_v<E>)
  = default;

  constexpr ExpectedImpl& operator=(const ExpectedImpl& rhs) noexcept {
    if (this != &rhs) {
      switch (rhs.state_) {
        case State::kValue:
          emplace_value(rhs.storage_.value);
          return *this;
        case State::kError:
          emplace_error(rhs.storage_.error);
          return *this;
        case State::kMovedFromValue:
        case State::kMovedFromError:
          break;
      }
      NOTREACHED();
    }
    return *this;
  }

  constexpr ExpectedImpl& operator=(ExpectedImpl&& rhs) noexcept {
    if (this != &rhs) {
      switch (rhs.state_) {
        case State::kValue:
          rhs.state_ = State::kMovedFromValue;
          emplace_value(std::move(rhs.storage_.value));
          return *this;
        case State::kError:
          rhs.state_ = State::kMovedFromError;
          emplace_error(std::move(rhs.storage_.error));
          return *this;
        case State::kMovedFromValue:
        case State::kMovedFromError:
          break;
      }
      NOTREACHED();
    }
    return *this;
  }

  template <typename... Args>
  constexpr T& emplace_value(Args&&... args) noexcept {
    DestroyStorage();
    state_ = State::kValue;
    return std::construct_at(&storage_, kValTag, std::forward<Args>(args)...)
        ->value;
  }

  template <typename U, typename... Args>
  constexpr T& emplace_value(std::initializer_list<U> il,
                             Args&&... args) noexcept {
    DestroyStorage();
    state_ = State::kValue;
    return std::construct_at(&storage_, kValTag, il,
                             std::forward<Args>(args)...)
        ->value;
  }

  template <typename... Args>
  constexpr E& emplace_error(Args&&... args) noexcept {
    DestroyStorage();
    state_ = State::kError;
    return std::construct_at(&storage_, kErrTag, std::forward<Args>(args)...)
        ->error;
  }

  template <typename U, typename... Args>
  constexpr E& emplace_error(std::initializer_list<U> il,
                             Args&&... args) noexcept {
    DestroyStorage();
    state_ = State::kError;
    return std::construct_at(&storage_, kErrTag, il,
                             std::forward<Args>(args)...)
        ->error;
  }

  void swap(ExpectedImpl& rhs) noexcept {
    using std::swap;

    CHECK(!is_moved_from());
    CHECK(!rhs.is_moved_from());
    if (state_ == rhs.state_) {
      switch (state_) {
        case State::kValue:
          swap(storage_.value, rhs.storage_.value);
          return;
        case State::kError:
          swap(storage_.error, rhs.storage_.error);
          return;
        case State::kMovedFromValue:
        case State::kMovedFromError:
          // This should never be reached; this condition should already be
          // caught above.
          break;
      }
      NOTREACHED();
    }
    ExpectedImpl tmp = std::move(*this);
    *this = std::move(rhs);
    rhs = std::move(tmp);
    return;
  }

  constexpr bool has_value() const noexcept {
    CHECK(!is_moved_from());
    return state_ == State::kValue;
  }

  constexpr T& value() noexcept {
    CHECK(state_ == State::kValue);
    return storage_.value;
  }
  constexpr const T& value() const noexcept {
    CHECK(state_ == State::kValue);
    return storage_.value;
  }
  constexpr E& error() noexcept {
    CHECK(state_ == State::kError);
    return storage_.error;
  }
  constexpr const E& error() const noexcept {
    CHECK(state_ == State::kError);
    return storage_.error;
  }

 private:
  // This avoids using std::variant because:
  // - the std::variant header is quite large
  // - but more importantly, it allows moved-from logic to be implemented
  //   without requiring extra storage. std::variant is insufficient here, as
  //   there are situations (e.g. rvalue conversions to a view type) where the
  //   storage for the original moved-from types needs to be retained.
  union Storage {
    constexpr Storage()
      requires(std::default_initializable<T>)
        : value() {}

    constexpr ~Storage()
      requires(std::is_trivially_destructible_v<T> &&
               std::is_trivially_destructible_v<E>)
    = default;
    constexpr ~Storage()
      requires(!std::is_trivially_destructible_v<T> ||
               !std::is_trivially_destructible_v<E>)
    {}

    template <typename... Args>
    constexpr explicit Storage(decltype(kValTag), Args&&... args)
        : value(std::forward<Args>(args)...) {}
    template <typename U, typename... Args>
    constexpr Storage(decltype(kValTag),
                      std::initializer_list<U> il,
                      Args&&... args)
        : value(il, std::forward<Args>(args)...) {}

    template <typename... Args>
    constexpr explicit Storage(decltype(kErrTag), Args&&... args)
        : error(std::forward<Args>(args)...) {}
    template <typename U, typename... Args>
    constexpr Storage(decltype(kErrTag),
                      std::initializer_list<U> il,
                      Args&&... args)
        : error(il, std::forward<Args>(args)...) {}

    T value;
    E error;
  };

  template <typename U, typename G>
  constexpr static Storage CreateStorageByCopy(const ExpectedImpl<U, G>& rhs) {
    switch (rhs.state_) {
      case State::kValue:
        return Storage(kValTag, rhs.storage_.value);
      case State::kError:
        return Storage(kErrTag, rhs.storage_.error);
      case State::kMovedFromValue:
      case State::kMovedFromError:
        break;
    }
    NOTREACHED();
  }

  template <typename U, typename G>
  constexpr static Storage CreateStorageByMove(ExpectedImpl<U, G>&& rhs) {
    switch (rhs.state_) {
      case State::kValue:
        rhs.state_ = State::kMovedFromValue;
        return Storage(kValTag, std::move(rhs.storage_.value));
      case State::kError:
        rhs.state_ = State::kMovedFromError;
        return Storage(kErrTag, std::move(rhs.storage_.error));
      case State::kMovedFromValue:
      case State::kMovedFromError:
        break;
    }
    NOTREACHED();
  }

  constexpr bool is_moved_from() const noexcept {
    return state_ > State::kError;
  }

  constexpr void DestroyStorage() {
    switch (state_) {
      case State::kValue:
      case State::kMovedFromValue:
        storage_.value.~T();
        break;
      case State::kError:
      case State::kMovedFromError:
        storage_.error.~E();
        break;
    }
  }

  State state_;
  Storage storage_;
};

template <typename Exp, typename F>
constexpr auto AndThen(Exp&& exp, F&& f) noexcept {
  using T = std::remove_cvref_t<decltype(exp.value())>;
  using E = std::remove_cvref_t<decltype(exp.error())>;

  auto invoke_f = [&]() -> decltype(auto) {
    if constexpr (!std::is_void_v<T>) {
      return std::invoke(std::forward<F>(f), std::forward<Exp>(exp).value());
    } else {
      return std::invoke(std::forward<F>(f));
    }
  };

  using U = decltype(invoke_f());
  static_assert(internal::IsExpected<U>,
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
  using T = std::remove_cvref_t<decltype(exp.value())>;
  using G = std::invoke_result_t<F, decltype(std::forward<Exp>(exp).error())>;

  static_assert(internal::IsExpected<G>,
                "expected<T, E>::or_else: Result of f() must be a "
                "specialization of expected");
  static_assert(
      std::is_same_v<typename G::value_type, T>,
      "expected<T, E>::or_else: Result of f() must have T as value_type");

  if (!exp.has_value()) {
    return std::invoke(std::forward<F>(f), std::forward<Exp>(exp).error());
  }

  if constexpr (!std::is_void_v<T>) {
    return G(std::in_place, std::forward<Exp>(exp).value());
  } else {
    return G();
  }
}

template <typename Exp, typename F>
constexpr auto Transform(Exp&& exp, F&& f) noexcept {
  using T = std::remove_cvref_t<decltype(exp.value())>;
  using E = std::remove_cvref_t<decltype(exp.error())>;

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
    static_assert(!std::is_same_v<U, std::in_place_t>,
                  "expected<T, E>::transform: Result of f() should "
                  "not be std::in_place_t");
    static_assert(!std::is_same_v<U, unexpect_t>,
                  "expected<T, E>::transform: Result of f() should "
                  "not be unexpect_t");
    static_assert(!internal::IsOk<U>,
                  "expected<T, E>::transform: Result of f() should "
                  "not be a specialization of ok");
    static_assert(!internal::IsUnexpected<U>,
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
    return expected<U, E>(std::in_place, invoke_f());
  } else {
    invoke_f();
    return expected<U, E>();
  }
}

template <typename Exp, typename F>
constexpr auto TransformError(Exp&& exp, F&& f) noexcept {
  using T = std::remove_cvref_t<decltype(exp.value())>;
  using G = std::remove_cv_t<
      std::invoke_result_t<F, decltype(std::forward<Exp>(exp).error())>>;

  static_assert(
      !std::is_array_v<G>,
      "expected<T, E>::transform_error: Result of f() should not be an Array");
  static_assert(!std::is_same_v<G, std::in_place_t>,
                "expected<T, E>::transform_error: Result of f() should not be "
                "std::in_place_t");
  static_assert(!std::is_same_v<G, unexpect_t>,
                "expected<T, E>::transform_error: Result of f() should not be "
                "unexpect_t");
  static_assert(!internal::IsOk<G>,
                "expected<T, E>::transform_error: Result of f() should not be "
                "a specialization of ok");
  static_assert(!internal::IsUnexpected<G>,
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
    return expected<T, G>(std::in_place, std::forward<Exp>(exp).value());
  }
}

}  // namespace internal

}  // namespace base

#endif  // BASE_TYPES_EXPECTED_INTERNAL_H_
