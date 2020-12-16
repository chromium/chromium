// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_INFRA_EXPECTED_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_INFRA_EXPECTED_H_

#include <type_traits>

#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

// TODO(b/172501195): Make these available outside namespace borealis.
namespace borealis {

// When signaling results that can succeed or fail, an Expected is used to
// formalize those two options either with a |T|, indicating success, or an |E|
// which indicates failure.
//
// This class is a non-drop-in-replacement for the std::expected, as that RFC is
// still pending (https://wg21.link/p0323r7).
template <typename T, typename E>
class Expected {
 public:
  // Convenient typedefs.
  using value_t = T;
  using error_t = E;

  // TODO(b/172501195): This implementation is only partial, either complete it
  // or replace it with the standard. Until then |T| and |E| should probably be
  // movable and copy-able respectively.
  static_assert(std::is_move_constructible<T>::value,
                "Expected's value type must be move-constructible");
  static_assert(std::is_copy_constructible<E>::value,
                "Expected's error type must be copy-constructible");
  static_assert(std::is_copy_assignable<E>::value,
                "Expected's error type must be copy-assignable");

  // Construct an object with the expected type.
  template <class TT>
  explicit Expected(TT&& value) : storage_(std::forward<T>(value)) {}

  // Construct an object with the error type.
  static Expected<T, E> Unexpected(E error) { return Expected(error); }

  // Returns true iff we are holding the expected value (i.e. a |T|).
  explicit operator bool() const {
    return absl::holds_alternative<T>(storage_);
  }

  // Convenience for negating the expectedness test.
  bool Unexpected() const { return !*this; }

  // Unsafe access to the underlying value. Use only if you know |this| is in
  // the requested state.
  T& Value() { return absl::get<T>(storage_); }
  const T& Value() const { return absl::get<T>(storage_); }

  // Unsafe access to the underlying error. Use only if you know |this| is in
  // the requested state.
  E& Error() { return absl::get<E>(storage_); }
  const E& Error() const { return absl::get<E>(storage_); }

  // Safe access to the underlying value, or nullptr;
  T* MaybeValue() { return absl::get_if<T>(&storage_); }

  // Safe access to the underlying error, or nullptr;
  E* MaybeError() { return absl::get_if<E>(&storage_); }

  // Invoke exactly one of the |on_value| or |on_error| callbacks, depending on
  // the state of |this|. Works a bit like absl::visit() but more chrome-ey.
  // Returns whatever type those callbacks return (which must be the same).
  template <typename R>
  R Handle(base::OnceCallback<R(T&)> on_value,
           base::OnceCallback<R(E&)> on_error) {
    if (*this) {
      return std::move(on_value).Run(absl::get<T>(storage_));
    } else {
      return std::move(on_error).Run(absl::get<E>(storage_));
    }
  }

 private:
  // We want people to explicitly use Unexpected(E) rather than this
  // constructor.
  explicit Expected(E error) : storage_(error) {}

  // Under-the-hood, the two states are recorded as a type-safe union.
  absl::variant<T, E> storage_;
};

// Convenience function for creating Expected<T, E> objects in the error state.
template <typename T, typename E>
Expected<T, E> Unexpected(E error) {
  return Expected<T, E>::Unexpected(error);
}

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_INFRA_EXPECTED_H_
