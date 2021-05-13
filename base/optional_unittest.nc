// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include <type_traits>

#include "base/optional.h"

namespace base {

#if defined(NCTEST_EXPLICIT_CONVERTING_COPY_CONSTRUCTOR)  // [r"fatal error: no matching function for call to object of type"]

// Optional<T>(const Optional<U>& arg) constructor is marked explicit if
// T is not convertible from "const U&".
void WontCompile() {
  struct Test {
    // Declares as explicit so that Test is still constructible from int,
    // but not convertible.
    explicit Test(int a) {}
  };

  static_assert(!std::is_convertible<const int&, Test>::value,
                "const int& to Test is convertible");
  const Optional<int> arg(in_place, 1);
  ([](Optional<Test> param) {})(arg);
}

#elif defined(NCTEST_EXPLICIT_CONVERTING_MOVE_CONSTRUCTOR)  // [r"fatal error: no matching function for call to object of type"]

// Optional<T>(Optional<U>&& arg) constructor is marked explicit if
// T is not convertible from "U&&".
void WontCompile() {
  struct Test {
    // Declares as explicit so that Test is still constructible from int,
    // but not convertible.
    explicit Test(int a) {}
  };

  static_assert(!std::is_convertible<int&&, Test>::value,
                "int&& to Test is convertible");
  ([](Optional<Test> param) {})(Optional<int>(in_place, 1));
}

#elif defined(NCTEST_EXPLICIT_VALUE_FORWARD_CONSTRUCTOR)  // [r"fatal error: no matching function for call to object of type"]

// Optional<T>(U&&) constructor is marked explicit if T is not convertible
// from U&&.
void WontCompile() {
  struct Test {
    // Declares as explicit so that Test is still constructible from int,
    // but not convertible.
    explicit Test(int a) {}
  };

  static_assert(!std::is_convertible<int&&, Test>::value,
                "int&& to Test is convertible");
  ([](Optional<Test> param) {})(1);
}

#elif defined(NCTEST_ILL_FORMED_IN_PLACET_T)  // [r"instantiation of base::Optional with in_place_t is ill-formed"]

// Optional<T> is ill-formed if T is `in_place_t`.
void WontCompile() {
  Optional<base::in_place_t> optional;
  optional.has_value();
}

#elif defined(NCTEST_ILL_FORMED_CONST_IN_PLACET_T)  // [r"instantiation of base::Optional with in_place_t is ill-formed"]

// Optional<T> is ill-formed if T is `const in_place_t`.
void WontCompile() {
  Optional<const base::in_place_t> optional;
  optional.has_value();
}

#elif defined(NCTEST_ILL_FORMED_NULLOPT_T)  // [r"instantiation of base::Optional with nullopt_t is ill-formed"]

// Optional<T> is ill-formed if T is `const nullopt_t`.
void WontCompile() {
  Optional<const base::nullopt_t> optional;
  optional.has_value();
}

#elif defined(NCTEST_ILL_FORMED_CONST_NULLOPT_T)  // [r"instantiation of base::Optional with nullopt_t is ill-formed"]

// Optional<T> is ill-formed if T is `const nullopt_t`.
void WontCompile() {
  Optional<const base::nullopt_t> optional;
  optional.has_value();
}

#elif defined(NCTEST_ILL_FORMED_NON_DESTRUCTIBLE)  // [r"instantiation of base::Optional with a non-destructible type is ill-formed"]

// Optional<T> is ill-formed if T is non-destructible.
void WontCompile() {
  struct T {
   private:
    ~T();
  };

  static_assert(!std::is_destructible<T>::value, "T is not destructible");

  Optional<T> optional;
  optional.has_value();
}

// TODO(crbug.com/967722): the error message should be about the instantiation of an
// ill-formed base::Optional.
#elif defined(NCTEST_ILL_FORMED_REFERENCE)  // [r"fatal error: union member 'value_' has reference type 'int &'"]

// Optional<T> is ill-formed if T is a reference.
void WontCompile() {
  using T = int&;

  static_assert(std::is_reference<T>::value, "T is a reference");

  Optional<T> optional;
  optional.has_value();
}

// TODO(crbug.com/967722): the error message should be about the instantiation of an
// ill-formed base::Optional.
#elif defined(NCTEST_ILL_FORMED_CONST_REFERENCE)  // [r"fatal error: union member 'value_' has reference type 'const int &'"]

// Optional<T> is ill-formed if T is a const reference.
void WontCompile() {
  using T = const int&;

  static_assert(std::is_reference<T>::value, "T is a reference");

  Optional<T> optional;
  optional.has_value();
}

#elif defined(NCTEST_ILL_FORMED_FIXED_LENGTH_ARRAY)  // [r"instantiation of base::Optional with an array type is ill-formed"]

// Optional<T> is ill-formed if T is a fixed length array.
void WontCompile() {
  using T = char[4];

  static_assert(std::is_array<T>::value, "T is an array");

  Optional<T> optional;
  optional.has_value();
}

// TODO(crbug.com/967722): the error message should be about the instantiation of an
// ill-formed base::Optional.
#elif defined(NCTEST_ILL_FORMED_UNDEFINED_LENGTH_ARRAY)  // [r"fatal error: base class 'OptionalStorageBase' has a flexible array member"]

// Optional<T> is ill-formed if T is a undefined length array.
void WontCompile() {
  using T = char[];

  static_assert(std::is_array<T>::value, "T is an array");

  Optional<T> optional;
  optional.has_value();
}

#endif

}  // namespace base
