// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"

#include <memory>
#include <optional>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Test with raw pointers.
TEST(CheckDerefTest, RawPointer) {
  int i = 123;
  int* ptr = &i;
  EXPECT_EQ(&i, &CHECK_DEREF(ptr));

  const int* const_ptr = &i;
  EXPECT_EQ(&i, &CHECK_DEREF(const_ptr));
}

TEST(CheckDerefTest, RawPointerDeath) {
  int* ptr = nullptr;
  BASE_EXPECT_DEATH((void)CHECK_DEREF(ptr), "");
}

// Test with a smart pointer.
TEST(CheckDerefTest, SmartPointer) {
  auto ptr = std::make_unique<int>(123);
  EXPECT_EQ(ptr.get(), &CHECK_DEREF(ptr));
}

TEST(CheckDerefTest, SmartPointerDeath) {
  std::unique_ptr<int> ptr;
  BASE_EXPECT_DEATH((void)CHECK_DEREF(ptr), "");
}

// Test with a function that returns a pointer.
int* ReturnNull() {
  return nullptr;
}
TEST(CheckDerefTest, FunctionCallDeath) {
  BASE_EXPECT_DEATH((void)CHECK_DEREF(ReturnNull()), "");
}

// Test with move only types.
TEST(CheckDerefTest, MoveOnlyOptionalUniquePtr) {
  auto ptr = std::make_unique<int>(42);
  int* raw_ptr = ptr.get();
  std::unique_ptr<int> result = CHECK_DEREF(std::optional(std::move(ptr)));
  EXPECT_EQ(result.get(), raw_ptr);
  EXPECT_EQ(*result, 42);
}

}  // namespace
