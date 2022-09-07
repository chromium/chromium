// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/function_ref.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

char Moo(float) {
  return 'a';
}

struct C {
  long Method() { return value; }
  long value;
};

}  // namespace

TEST(FunctionRefTest, FreeFunction) {
  [](FunctionRef<char(float)> ref) { EXPECT_EQ('a', ref(1.0)); }(&Moo);
}

TEST(FunctionRefTest, Method) {
  [](FunctionRef<long(C*)> ref) {
    C c = {.value = 25L};
    EXPECT_EQ(25L, ref(&c));
  }(&C::Method);
}

TEST(FunctionRefTest, Lambda) {
  int x = 3;
  auto lambda = [&x]() { return x; };
  [](FunctionRef<int()> ref) { EXPECT_EQ(3, ref()); }(lambda);
}

TEST(FunctionRefTest, AbslConversion) {
  // Matching signatures should work.
  {
    bool called = false;
    auto lambda = [&called](float) {
      called = true;
      return 'a';
    };
    FunctionRef<char(float)> ref(lambda);
    [](absl::FunctionRef<char(float)> absl_ref) {
      absl_ref(1.0);
    }(ref.ToAbsl());
    EXPECT_TRUE(called);
  }

  // `absl::FunctionRef` should be able to adapt "similar enough" signatures.
  {
    bool called = false;
    auto lambda = [&called](float) {
      called = true;
      return 'a';
    };
    FunctionRef<char(float)> ref(lambda);
    [](absl::FunctionRef<void(float)> absl_ref) {
      absl_ref(1.0);
    }(ref.ToAbsl());
    EXPECT_TRUE(called);
  }
}

}  // namespace base
