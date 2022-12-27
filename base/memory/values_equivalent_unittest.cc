// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/values_equivalent.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ValuesEquivalentTest, Comparisons) {
  int a = 1234;
  int b1 = 5678;
  int b2 = 5678;

  EXPECT_TRUE(ValuesEquivalent<int>(nullptr, nullptr));
  EXPECT_FALSE(ValuesEquivalent<int>(&a, nullptr));
  EXPECT_FALSE(ValuesEquivalent<int>(nullptr, &a));
  EXPECT_FALSE(ValuesEquivalent(&a, &b1));
  EXPECT_TRUE(ValuesEquivalent(&a, &a));
  EXPECT_TRUE(ValuesEquivalent(&b1, &b2));
}

TEST(ValuesEquivalentTest, UniquePtr) {
  auto a = std::make_unique<int>(1234);
  auto b1 = std::make_unique<int>(5678);
  auto b2 = std::make_unique<int>(5678);

  EXPECT_TRUE(ValuesEquivalent(std::unique_ptr<int>(), std::unique_ptr<int>()));
  EXPECT_FALSE(ValuesEquivalent(a, std::unique_ptr<int>()));
  EXPECT_FALSE(ValuesEquivalent(std::unique_ptr<int>(), a));
  EXPECT_FALSE(ValuesEquivalent(a, b1));
  EXPECT_TRUE(ValuesEquivalent(a, a));
  EXPECT_TRUE(ValuesEquivalent(b1, b2));
}

TEST(ValuesEquivalentTest, ScopedRefPtr) {
  struct Wrapper : public RefCounted<Wrapper> {
    explicit Wrapper(int value) : value(value) {}
    int value;
    bool operator==(const Wrapper& other) const { return value == other.value; }

   protected:
    friend class RefCounted<Wrapper>;
    virtual ~Wrapper() = default;
  };

  auto a = MakeRefCounted<Wrapper>(1234);
  auto b1 = MakeRefCounted<Wrapper>(5678);
  auto b2 = MakeRefCounted<Wrapper>(5678);

  EXPECT_TRUE(
      ValuesEquivalent(scoped_refptr<Wrapper>(), scoped_refptr<Wrapper>()));
  EXPECT_FALSE(ValuesEquivalent(a, scoped_refptr<Wrapper>()));
  EXPECT_FALSE(ValuesEquivalent(scoped_refptr<Wrapper>(), a));
  EXPECT_FALSE(ValuesEquivalent(a, b1));
  EXPECT_TRUE(ValuesEquivalent(a, a));
  EXPECT_TRUE(ValuesEquivalent(b1, b2));
}

TEST(ValuesEquivalentTest, CapitalGetPtr) {
  class IntPointer {
   public:
    explicit IntPointer(int* pointer) : pointer_(pointer) {}
    const int* Get() const { return pointer_; }

   private:
    raw_ptr<int> pointer_ = nullptr;
  };

  auto a = 1234;
  auto b1 = 5678;
  auto b2 = 5678;

  EXPECT_TRUE(ValuesEquivalent(IntPointer(nullptr), IntPointer(nullptr)));
  EXPECT_FALSE(ValuesEquivalent(IntPointer(&a), IntPointer(nullptr)));
  EXPECT_FALSE(ValuesEquivalent(IntPointer(nullptr), IntPointer(&a)));
  EXPECT_FALSE(ValuesEquivalent(IntPointer(&a), IntPointer(&b1)));
  EXPECT_TRUE(ValuesEquivalent(IntPointer(&a), IntPointer(&a)));
  EXPECT_TRUE(ValuesEquivalent(IntPointer(&b1), IntPointer(&b2)));
}

TEST(ValuesEquivalentTest, BypassEqualsOperator) {
  struct NeverEqual {
    bool operator==(const NeverEqual& other) const { return false; }
  } a, b;

  ASSERT_FALSE(a == a);
  ASSERT_FALSE(a == b);

  EXPECT_TRUE(ValuesEquivalent(&a, &a));
  EXPECT_FALSE(ValuesEquivalent(&a, &b));
}

TEST(ValuesEquavalentTest, Predicate) {
  auto is_same_or_next = [](int a, int b) { return a == b || a == b + 1; };
  int x = 1;
  int y = 2;
  int z = 3;

  EXPECT_TRUE(ValuesEquivalent(&x, &x, is_same_or_next));
  EXPECT_FALSE(ValuesEquivalent(&x, &y, is_same_or_next));
  EXPECT_FALSE(ValuesEquivalent(&x, &z, is_same_or_next));
  EXPECT_TRUE(ValuesEquivalent(&y, &x, is_same_or_next));
  EXPECT_FALSE(ValuesEquivalent(&y, &z, is_same_or_next));
  EXPECT_FALSE(ValuesEquivalent(&z, &x, is_same_or_next));
  EXPECT_TRUE(ValuesEquivalent(&z, &y, is_same_or_next));
  EXPECT_TRUE(ValuesEquivalent<int>(nullptr, nullptr, is_same_or_next));
  EXPECT_FALSE(ValuesEquivalent<int>(&x, nullptr, is_same_or_next));
  EXPECT_FALSE(ValuesEquivalent<int>(nullptr, &x, is_same_or_next));
}

}  // namespace base
