// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_EXPECTED_H_
#define BASE_TYPES_EXPECTED_H_

#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/types/expected_internal.h"  // IWYU pragma: export
#include "third_party/abseil-cpp/absl/utility/utility.h"

// Class template `expected<T, E>` is a vocabulary type which contains an
// expected value of type `T`, or an error `E`. The class skews towards behaving
// like a `T`, because its intended use is when the expected type is contained.
// When something unexpected occurs, more typing is required. When all is good,
// code mostly looks as if a `T` were being handled.
//
// Class template `expected<T, E>` contains either:
// * A value of type `T`, the expected value type; or
// * A value of type `E`, an error type used when an unexpected outcome
// occurred.
//
// The interface can be queried as to whether the underlying value is the
// expected value (of type `T`) or an unexpected value (of type `E`). The
// interface and the rational are based on `std::optional`. We consider
// `expected<T, E>` as a supplement to `optional<T>`, expressing why an expected
// value isn’t contained in the object.
//
// Example Usage:
//
// Before:
//   bool ParseInt32(base::StringPiece input,
//                   int32_t* output,
//                   ParseIntError* error);
//   ...
//
//   int32_t output;
//   ParseIntError error;
//   if (ParseInt32("...". &output, &error)) {
//     // process `output`
//   } else {
//     // process `error`
//   }
//
// After:
//
//   base::expected<int32_t, ParseIntError> ParseInt32(base::StringPiece input);
//   ...
//
//   if (auto parsed = ParseInt32("..."); parsed.has_value()) {
//     // process `parsed.value()`
//   } else {
//     // process `parsed.error()`
//   }
//
// References:
// * https://wg21.link/P0323
// * https://eel.is/c++draft/expected
namespace base {

// Note: base::unexpected and base::expected are C++17 compatible backports of
// C++23's std::unexpected and std::expected. They differ from the standard in
// the following ways:
//
// * Not all member functions can be used in a constexpr context. This is due to
//   limitations in both the C++17 language and the Abseil library used for the
//   implementation.
// * Since Chromium does not support exceptions, there is no bad_expected_access
//   exception and the program will just terminate when the exception would have
//   been thrown. Furthermore, all member functions are marked noexcept.
// * C++23 allows an implicit conversion from U to expected<T, E> if U is
//   implicitly convertible to T; the Chromium version only allows an implicit
//   conversion if U is implicitly convertible to T *and* U is *not* implicitly
//   convertible to E, to guard against bug-prone patterns such as:
//     // creates an expected value containing true, not an unexpected value
//     // containing 123L.
//     expected<bool, long> e = 123L;
// * Because of the above restriction, the Chromium version also introduces
//   `base::ok` as a complement to `base::unexpected` to simplify returning
//   success values when the implicit conversion above is disallowed.
// * Calling operator* or operator-> on an unexpected value results in program
//   termination, and not UB.
// * There is no operator bool due to bug-prone usage when the value type is
//   convertible to bool, see e.g. https://abseil.io/tips/141.
// * Moving out of an expected object will put it into a moved-from state.
//   Trying to use it before re-initializing it will result in program
//   termination.
// * The expected<void> specialization is done via a defaulted boolean template
//   parameter, due to the lack of requires clauses in C++17.
// * Since equality operators can not be defaulted in C++17, equality and
//   inequality operators are specified explicitly.

// Class template used as a type hint for constructing a `base::expected`
// containing a value (i.e. success). Useful when implicit conversion
// construction of `base::expected` is disallowed, e.g. due to ambiguity.
// Example usage:
//
//   base::expected<std::string, std::string> RunOp() {
//     std::string error;
//     std::string result = RunOpImpl(..., &error);
//     if (!error.empty()) {
//       return base::unexpected(std::move(error));
//
//     // The C++23 std::expected proposal allows this to be simply written as
//     //   return result;
//     //
//     // However, the Chromium version disallows this if E implicitly converts
//     // to T, so without base::ok(), this would have to be written as:
//     //   return base::expected<std::string, std::string>(std::move(result));
//
//     return base::ok(std::move(result));
//   }
template <typename T>
class ok {
 public:
  template <typename U = T, internal::EnableIfOkValueConstruction<T, U> = 0>
  constexpr explicit ok(U&& val) noexcept : value_(std::forward<U>(val)) {}

  template <typename... Args>
  constexpr explicit ok(absl::in_place_t, Args&&... args) noexcept
      : value_(std::forward<Args>(args)...) {}

  template <typename U, typename... Args>
  constexpr explicit ok(absl::in_place_t,
                        std::initializer_list<U> il,
                        Args&&... args) noexcept
      : value_(il, std::forward<Args>(args)...) {}

  constexpr T& value() & noexcept { return value_; }
  constexpr const T& value() const& noexcept { return value_; }
  constexpr T&& value() && noexcept { return std::move(value()); }
  constexpr const T&& value() const&& noexcept { return std::move(value()); }

  constexpr void swap(ok& other) noexcept {
    using std::swap;
    swap(value(), other.value());
  }

  friend constexpr void swap(ok& x, ok& y) noexcept { x.swap(y); }

 private:
  T value_;
};

template <typename T, typename U>
constexpr bool operator==(const ok<T>& lhs, const ok<U>& rhs) noexcept {
  return lhs.value() == rhs.value();
}

template <typename T, typename U>
constexpr bool operator!=(const ok<T>& lhs, const ok<U>& rhs) noexcept {
  return !(lhs == rhs);
}

template <typename T>
ok(T) -> ok<T>;

// [expected.un.object], class template unexpected
// https://eel.is/c++draft/expected#un.object
template <typename E>
class unexpected {
 public:
  // [expected.un.ctor] Constructors
  template <typename Err = E,
            internal::EnableIfUnexpectedValueConstruction<E, Err> = 0>
  constexpr explicit unexpected(Err&& err) noexcept
      : error_(std::forward<Err>(err)) {}

  template <typename... Args>
  constexpr explicit unexpected(absl::in_place_t, Args&&... args) noexcept
      : error_(std::forward<Args>(args)...) {}

  template <typename U, typename... Args>
  constexpr explicit unexpected(absl::in_place_t,
                                std::initializer_list<U> il,
                                Args&&... args) noexcept
      : error_(il, std::forward<Args>(args)...) {}

  // [expected.un.obs] Observers
  constexpr E& error() & noexcept { return error_; }
  constexpr const E& error() const& noexcept { return error_; }
  constexpr E&& error() && noexcept { return std::move(error()); }
  constexpr const E&& error() const&& noexcept { return std::move(error()); }

  // [expected.un.swap] Swap
  constexpr void swap(unexpected& other) noexcept {
    using std::swap;
    swap(error(), other.error());
  }

  friend constexpr void swap(unexpected& x, unexpected& y) noexcept {
    x.swap(y);
  }

 private:
  E error_;
};

// [expected.un.eq] Equality operator
template <typename E, typename G>
constexpr bool operator==(const unexpected<E>& lhs,
                          const unexpected<G>& rhs) noexcept {
  return lhs.error() == rhs.error();
}

template <typename E, typename G>
constexpr bool operator!=(const unexpected<E>& lhs,
                          const unexpected<G>& rhs) noexcept {
  return !(lhs == rhs);
}

template <typename E>
unexpected(E) -> unexpected<E>;

// in-place construction of unexpected values
struct unexpect_t {
  explicit unexpect_t() = default;
};
inline constexpr unexpect_t unexpect{};

// [expected.expected], class template expected
// https://eel.is/c++draft/expected#expected
template <typename T, typename E>
class expected<T, E, /* is_void_v<T> = */ false> {
  // Note: A partial specialization for void value types follows below.
  static_assert(!std::is_void_v<T>, "Error: T must not be void");

 public:
  using value_type = T;
  using error_type = E;
  using unexpected_type = unexpected<E>;

  // Alias template to explicitly opt into the std::pointer_traits machinery.
  // See e.g. https://en.cppreference.com/w/cpp/memory/pointer_traits#Notes
  template <typename U>
  using rebind = expected<U, E>;

  template <typename U, typename G, bool IsVoid>
  friend class expected;

  // [expected.object.ctor], constructors
  constexpr expected() noexcept = default;

  // Converting copy and move constructors. These constructors are explicit if
  // either the value or error type is not implicitly convertible from `rhs`'s
  // corresponding type.
  template <typename U,
            typename G,
            internal::EnableIfExplicitConversion<T, E, const U&, const G&> = 0>
  explicit constexpr expected(const expected<U, G>& rhs) noexcept
      : impl_(rhs.impl_) {}

  template <typename U,
            typename G,
            internal::EnableIfImplicitConversion<T, E, const U&, const G&> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ constexpr expected(const expected<U, G>& rhs) noexcept
      : impl_(rhs.impl_) {}

  template <typename U,
            typename G,
            internal::EnableIfExplicitConversion<T, E, U, G> = 0>
  explicit constexpr expected(expected<U, G>&& rhs) noexcept
      : impl_(std::move(rhs.impl_)) {}

  template <typename U,
            typename G,
            internal::EnableIfImplicitConversion<T, E, U, G> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ constexpr expected(expected<U, G>&& rhs) noexcept
      : impl_(std::move(rhs.impl_)) {}

  // Deviation from the Standard, which allows implicit conversions as long as U
  // is implicitly convertible to T: Chromium additionally requires that U is
  // not implicitly convertible to E.
  template <typename U = T,
            internal::EnableIfExplicitValueConstruction<T, E, U> = 0>
  explicit constexpr expected(U&& v) noexcept
      : impl_(kValTag, std::forward<U>(v)) {}

  template <typename U = T,
            internal::EnableIfImplicitValueConstruction<T, E, U> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ constexpr expected(U&& v) noexcept
      : impl_(kValTag, std::forward<U>(v)) {}

  template <typename U, internal::EnableIfExplicitConstruction<T, const U&> = 0>
  explicit constexpr expected(const ok<U>& o) noexcept
      : impl_(kValTag, o.value()) {}

  template <typename U, internal::EnableIfImplicitConstruction<T, const U&> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ constexpr expected(const ok<U>& o) noexcept
      : impl_(kValTag, o.value()) {}

  template <typename U, internal::EnableIfExplicitConstruction<T, U> = 0>
  explicit constexpr expected(ok<U>&& o) noexcept
      : impl_(kValTag, std::move(o.value())) {}

  template <typename U, internal::EnableIfImplicitConstruction<T, U> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ constexpr expected(ok<U>&& o) noexcept
      : impl_(kValTag, std::move(o.value())) {}

  template <typename G, internal::EnableIfExplicitConstruction<E, const G&> = 0>
  explicit constexpr expected(const unexpected<G>& e) noexcept
      : impl_(kErrTag, e.error()) {}

  template <typename G, internal::EnableIfImplicitConstruction<E, const G&> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ constexpr expected(const unexpected<G>& e) noexcept
      : impl_(kErrTag, e.error()) {}

  template <typename G, internal::EnableIfExplicitConstruction<E, G> = 0>
  explicit constexpr expected(unexpected<G>&& e) noexcept
      : impl_(kErrTag, std::move(e.error())) {}

  template <typename G, internal::EnableIfImplicitConstruction<E, G> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ constexpr expected(unexpected<G>&& e) noexcept
      : impl_(kErrTag, std::move(e.error())) {}

  template <typename... Args>
  constexpr explicit expected(absl::in_place_t, Args&&... args) noexcept
      : impl_(kValTag, std::forward<Args>(args)...) {}

  template <typename U, typename... Args>
  constexpr explicit expected(absl::in_place_t,
                              std::initializer_list<U> il,
                              Args&&... args) noexcept
      : impl_(kValTag, il, std::forward<Args>(args)...) {}

  template <typename... Args>
  constexpr explicit expected(unexpect_t, Args&&... args) noexcept
      : impl_(kErrTag, std::forward<Args>(args)...) {}

  template <typename U, typename... Args>
  constexpr explicit expected(unexpect_t,
                              std::initializer_list<U> il,
                              Args&&... args) noexcept
      : impl_(kErrTag, il, std::forward<Args>(args)...) {}

  // [expected.object.assign], assignment
  template <typename U = T, internal::EnableIfValueAssignment<T, E, U> = 0>
  constexpr expected& operator=(U&& v) noexcept {
    emplace(std::forward<U>(v));
    return *this;
  }

  template <typename U>
  constexpr expected& operator=(const ok<U>& o) noexcept {
    emplace(o.value());
    return *this;
  }

  template <typename U>
  constexpr expected& operator=(ok<U>&& o) noexcept {
    emplace(std::move(o.value()));
    return *this;
  }

  template <typename G>
  constexpr expected& operator=(const unexpected<G>& e) noexcept {
    impl_.emplace_error(e.error());
    return *this;
  }

  template <typename G>
  constexpr expected& operator=(unexpected<G>&& e) noexcept {
    impl_.emplace_error(std::move(e.error()));
    return *this;
  }

  template <typename... Args>
  constexpr T& emplace(Args&&... args) noexcept {
    return impl_.emplace_value(std::forward<Args>(args)...);
  }

  template <typename U, typename... Args>
  constexpr T& emplace(std::initializer_list<U> il, Args&&... args) noexcept {
    return impl_.emplace_value(il, std::forward<Args>(args)...);
  }

  // [expected.object.swap], swap
  constexpr void swap(expected& rhs) noexcept { impl_.swap(rhs.impl_); }
  friend constexpr void swap(expected& x, expected& y) noexcept { x.swap(y); }

  // [expected.object.obs], observers
  constexpr T* operator->() noexcept { return std::addressof(value()); }
  constexpr const T* operator->() const noexcept {
    return std::addressof(value());
  }

  constexpr T& operator*() & noexcept { return value(); }
  constexpr const T& operator*() const& noexcept { return value(); }
  constexpr T&& operator*() && noexcept { return std::move(value()); }
  constexpr const T&& operator*() const&& noexcept {
    return std::move(value());
  }

  // Note: Deviation from the Standard: No operator bool due to bug-prone
  // patterns when the value type is convertible to bool, see e.g.
  // https://abseil.io/tips/141.
  constexpr bool has_value() const noexcept { return impl_.has_value(); }

  constexpr T& value() & noexcept { return impl_.value(); }
  constexpr const T& value() const& noexcept { return impl_.value(); }
  constexpr T&& value() && noexcept { return std::move(value()); }
  constexpr const T&& value() const&& noexcept { return std::move(value()); }

  constexpr E& error() & noexcept { return impl_.error(); }
  constexpr const E& error() const& noexcept { return impl_.error(); }
  constexpr E&& error() && noexcept { return std::move(error()); }
  constexpr const E&& error() const&& noexcept { return std::move(error()); }

  template <typename U>
  constexpr T value_or(U&& v) const& noexcept {
    static_assert(std::is_copy_constructible_v<T>,
                  "expected<T, E>::value_or: T must be copy constructible");
    static_assert(std::is_convertible_v<U&&, T>,
                  "expected<T, E>::value_or: U must be convertible to T");
    return has_value() ? value() : static_cast<T>(std::forward<U>(v));
  }

  template <typename U>
  constexpr T value_or(U&& v) && noexcept {
    static_assert(std::is_move_constructible_v<T>,
                  "expected<T, E>::value_or: T must be move constructible");
    static_assert(std::is_convertible_v<U&&, T>,
                  "expected<T, E>::value_or: U must be convertible to T");
    return has_value() ? std::move(value())
                       : static_cast<T>(std::forward<U>(v));
  }

 private:
  using Impl = internal::ExpectedImpl<T, E>;
  static constexpr auto kValTag = Impl::kValTag;
  static constexpr auto kErrTag = Impl::kErrTag;

  Impl impl_;
};

// [expected.void], partial specialization of expected for void types
template <typename T, typename E>
class expected<T, E, /* is_void_v<T> = */ true> {
  // Note: A partial specialization for non-void value types can be found above.
  static_assert(std::is_void_v<T>, "Error: T must be void");

 public:
  using value_type = T;
  using error_type = E;
  using unexpected_type = unexpected<E>;

  // Alias template to explicitly opt into the std::pointer_traits machinery.
  // See e.g. https://en.cppreference.com/w/cpp/memory/pointer_traits#Notes
  template <typename U>
  using rebind = expected<U, E>;

  template <typename U, typename G, bool IsVoid>
  friend class expected;

  // [expected.void.ctor], constructors
  constexpr expected() noexcept = default;

  // Converting copy and move constructors. These constructors are explicit if
  // the error type is not implicitly convertible from `rhs`'s error type.
  template <typename U,
            typename G,
            internal::EnableIfExplicitVoidConversion<E, U, const G&> = 0>
  constexpr explicit expected(const expected<U, G>& rhs) noexcept
      : impl_(rhs.impl_) {}

  template <typename U,
            typename G,
            internal::EnableIfImplicitVoidConversion<E, U, const G&> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr /* implicit */ expected(const expected<U, G>& rhs) noexcept
      : impl_(rhs.impl_) {}

  template <typename U,
            typename G,
            internal::EnableIfExplicitVoidConversion<E, U, G> = 0>
  constexpr explicit expected(expected<U, G>&& rhs) noexcept
      : impl_(std::move(rhs.impl_)) {}

  template <typename U,
            typename G,
            internal::EnableIfImplicitVoidConversion<E, U, G> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr /* implicit */ expected(expected<U, G>&& rhs) noexcept
      : impl_(std::move(rhs.impl_)) {}

  template <typename G, internal::EnableIfExplicitConstruction<E, const G&> = 0>
  explicit constexpr expected(const unexpected<G>& e) noexcept
      : impl_(kErrTag, e.error()) {}

  template <typename G, internal::EnableIfImplicitConstruction<E, const G&> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ constexpr expected(const unexpected<G>& e) noexcept
      : impl_(kErrTag, e.error()) {}

  template <typename G, internal::EnableIfExplicitConstruction<E, G> = 0>
  explicit constexpr expected(unexpected<G>&& e) noexcept
      : impl_(kErrTag, std::move(e.error())) {}

  template <typename G, internal::EnableIfImplicitConstruction<E, G> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ constexpr expected(unexpected<G>&& e) noexcept
      : impl_(kErrTag, std::move(e.error())) {}

  constexpr explicit expected(absl::in_place_t) noexcept {}

  template <typename... Args>
  constexpr explicit expected(unexpect_t, Args&&... args) noexcept
      : impl_(kErrTag, std::forward<Args>(args)...) {}

  template <typename U, typename... Args>
  constexpr explicit expected(unexpect_t,
                              std::initializer_list<U> il,
                              Args&&... args) noexcept
      : impl_(kErrTag, il, std::forward<Args>(args)...) {}

  // [expected.void.assign], assignment
  template <typename G>
  constexpr expected& operator=(const unexpected<G>& e) noexcept {
    impl_.emplace_error(e.error());
    return *this;
  }

  template <typename G>
  constexpr expected& operator=(unexpected<G>&& e) noexcept {
    impl_.emplace_error(std::move(e.error()));
    return *this;
  }

  constexpr void emplace() noexcept { impl_.emplace_value(); }

  // [expected.void.swap], swap
  constexpr void swap(expected& rhs) noexcept { impl_.swap(rhs.impl_); }
  friend constexpr void swap(expected& x, expected& y) noexcept { x.swap(y); }

  // [expected.void.obs], observers
  // Note: Deviation from the Standard: No operator bool due to consistency with
  // non-void expected types.
  constexpr bool has_value() const noexcept { return impl_.has_value(); }

  constexpr void operator*() const { CHECK(has_value()); }
  constexpr void value() const { CHECK(has_value()); }

  constexpr E& error() & { return impl_.error(); }
  constexpr const E& error() const& { return impl_.error(); }
  constexpr E&& error() && { return std::move(error()); }
  constexpr const E&& error() const&& { return std::move(error()); }

 private:
  // Note: Since we can't store void types we use absl::monostate instead.
  using Impl = internal::ExpectedImpl<absl::monostate, E>;
  static constexpr auto kErrTag = Impl::kErrTag;

  Impl impl_;
};

// [expected.object.eq], equality operators
// [expected.void.eq], equality operators
template <typename T, typename E, typename U, typename G, bool IsVoid>
constexpr bool operator==(const expected<T, E, IsVoid>& x,
                          const expected<U, G, IsVoid>& y) noexcept {
  auto equal_values = [](const auto& x, const auto& y) {
    // Values for expected void types always compare equal.
    if constexpr (IsVoid) {
      return true;
    } else {
      return x.value() == y.value();
    }
  };

  return x.has_value() == y.has_value() &&
         (x.has_value() ? equal_values(x, y) : x.error() == y.error());
}

template <typename T, typename E, typename U, internal::EnableIfNotVoid<T> = 0>
constexpr bool operator==(const expected<T, E>& x, const U& v) noexcept {
  return x.has_value() && x.value() == v;
}

template <typename T, typename E, typename U, internal::EnableIfNotVoid<T> = 0>
constexpr bool operator==(const U& v, const expected<T, E>& x) noexcept {
  return x == v;
}

template <typename T, typename E, typename U, internal::EnableIfNotVoid<T> = 0>
constexpr bool operator==(const expected<T, E>& x, const ok<U>& o) noexcept {
  return x.has_value() && x.value() == o.value();
}

template <typename T, typename E, typename U, internal::EnableIfNotVoid<T> = 0>
constexpr bool operator==(const ok<U>& o, const expected<T, E>& x) noexcept {
  return x == o;
}

template <typename T, typename E, typename G>
constexpr bool operator==(const expected<T, E>& x,
                          const unexpected<G>& e) noexcept {
  return !x.has_value() && x.error() == e.error();
}

template <typename T, typename E, typename G>
constexpr bool operator==(const unexpected<G>& e,
                          const expected<T, E>& x) noexcept {
  return x == e;
}

template <typename T, typename E, typename U, typename G, bool IsVoid>
constexpr bool operator!=(const expected<T, E, IsVoid>& x,
                          const expected<U, G, IsVoid>& y) noexcept {
  return !(x == y);
}

template <typename T, typename E, typename U, internal::EnableIfNotVoid<T> = 0>
constexpr bool operator!=(const expected<T, E>& x, const U& v) noexcept {
  return !(x == v);
}

template <typename T, typename E, typename U, internal::EnableIfNotVoid<T> = 0>
constexpr bool operator!=(const U& v, const expected<T, E>& x) noexcept {
  return !(v == x);
}

template <typename T, typename E, typename U, internal::EnableIfNotVoid<T> = 0>
constexpr bool operator!=(const expected<T, E>& x, const ok<U>& o) noexcept {
  return !(x == o);
}

template <typename T, typename E, typename U, internal::EnableIfNotVoid<T> = 0>
constexpr bool operator!=(const ok<U>& o, const expected<T, E>& x) noexcept {
  return !(o == x);
}

template <typename T, typename E, typename G>
constexpr bool operator!=(const expected<T, E>& x,
                          const unexpected<G>& e) noexcept {
  return !(x == e);
}

template <typename T, typename E, typename G>
constexpr bool operator!=(const unexpected<G>& e,
                          const expected<T, E>& x) noexcept {
  return !(e == x);
}

}  // namespace base

#endif  // BASE_TYPES_EXPECTED_H_
