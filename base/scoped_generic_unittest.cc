// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_generic.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

struct IntTraits {
  IntTraits(std::vector<int>* freed) : freed_ints(freed) {}

  static int InvalidValue() {
    return -1;
  }
  void Free(int value) {
    freed_ints->push_back(value);
  }

  std::vector<int>* freed_ints;
};

using ScopedInt = ScopedGeneric<int, IntTraits>;

}  // namespace

TEST(ScopedGenericTest, ScopedGeneric) {
  std::vector<int> values_freed;
  IntTraits traits(&values_freed);

  // Invalid case, delete should not be called.
  {
    ScopedInt a(IntTraits::InvalidValue(), traits);
  }
  EXPECT_TRUE(values_freed.empty());

  // Simple deleting case.
  static const int kFirst = 0;
  {
    ScopedInt a(kFirst, traits);
  }
  ASSERT_EQ(1u, values_freed.size());
  ASSERT_EQ(kFirst, values_freed[0]);
  values_freed.clear();

  // Release should return the right value and leave the object empty.
  {
    ScopedInt a(kFirst, traits);
    EXPECT_EQ(kFirst, a.release());

    ScopedInt b(IntTraits::InvalidValue(), traits);
    EXPECT_EQ(IntTraits::InvalidValue(), b.release());
  }
  ASSERT_TRUE(values_freed.empty());

  // Reset should free the old value, then the new one should go away when
  // it goes out of scope.
  static const int kSecond = 1;
  {
    ScopedInt b(kFirst, traits);
    b.reset(kSecond);
    ASSERT_EQ(1u, values_freed.size());
    ASSERT_EQ(kFirst, values_freed[0]);
  }
  ASSERT_EQ(2u, values_freed.size());
  ASSERT_EQ(kSecond, values_freed[1]);
  values_freed.clear();

  // Swap.
  {
    ScopedInt a(kFirst, traits);
    ScopedInt b(kSecond, traits);
    a.swap(b);
    EXPECT_TRUE(values_freed.empty());  // Nothing should be freed.
    EXPECT_EQ(kSecond, a.get());
    EXPECT_EQ(kFirst, b.get());
  }
  // Values should be deleted in the opposite order.
  ASSERT_EQ(2u, values_freed.size());
  EXPECT_EQ(kFirst, values_freed[0]);
  EXPECT_EQ(kSecond, values_freed[1]);
  values_freed.clear();

  // Move constructor.
  {
    ScopedInt a(kFirst, traits);
    ScopedInt b(std::move(a));
    EXPECT_TRUE(values_freed.empty());  // Nothing should be freed.
    ASSERT_EQ(IntTraits::InvalidValue(), a.get());
    ASSERT_EQ(kFirst, b.get());
  }

  ASSERT_EQ(1u, values_freed.size());
  ASSERT_EQ(kFirst, values_freed[0]);
  values_freed.clear();

  // Move assign.
  {
    ScopedInt a(kFirst, traits);
    ScopedInt b(kSecond, traits);
    b = std::move(a);
    ASSERT_EQ(1u, values_freed.size());
    EXPECT_EQ(kSecond, values_freed[0]);
    ASSERT_EQ(IntTraits::InvalidValue(), a.get());
    ASSERT_EQ(kFirst, b.get());
  }

  ASSERT_EQ(2u, values_freed.size());
  EXPECT_EQ(kFirst, values_freed[1]);
  values_freed.clear();
}

TEST(ScopedGenericTest, Operators) {
  std::vector<int> values_freed;
  IntTraits traits(&values_freed);

  static const int kFirst = 0;
  static const int kSecond = 1;
  {
    ScopedInt a(kFirst, traits);
    EXPECT_TRUE(a == kFirst);
    EXPECT_FALSE(a != kFirst);
    EXPECT_FALSE(a == kSecond);
    EXPECT_TRUE(a != kSecond);

    EXPECT_TRUE(kFirst == a);
    EXPECT_FALSE(kFirst != a);
    EXPECT_FALSE(kSecond == a);
    EXPECT_TRUE(kSecond != a);
  }

  // is_valid().
  {
    ScopedInt a(kFirst, traits);
    EXPECT_TRUE(a.is_valid());
    a.reset();
    EXPECT_FALSE(a.is_valid());
  }
}

TEST(ScopedGenericTest, Receive) {
  std::vector<int> values_freed;
  IntTraits traits(&values_freed);
  auto a = std::make_unique<ScopedInt>(123, traits);

  EXPECT_EQ(123, a->get());

  {
    ScopedInt::Receiver r(*a);
    EXPECT_EQ(123, a->get());
    *r.get() = 456;
    EXPECT_EQ(123, a->get());
  }

  EXPECT_EQ(456, a->get());

  {
    ScopedInt::Receiver r(*a);
    EXPECT_DEATH_IF_SUPPORTED(a.reset(), "");
    EXPECT_DEATH_IF_SUPPORTED(ScopedInt::Receiver(*a).get(), "");
  }
}

namespace {

struct TrackedIntTraits : public ScopedGenericOwnershipTracking {
  using OwnerMap =
      std::unordered_map<int, const ScopedGeneric<int, TrackedIntTraits>*>;
  TrackedIntTraits(std::unordered_set<int>* freed, OwnerMap* owners)
      : freed(freed), owners(owners) {}

  static int InvalidValue() { return -1; }

  void Free(int value) {
    auto it = owners->find(value);
    ASSERT_EQ(owners->end(), it);

    ASSERT_EQ(0U, freed->count(value));
    freed->insert(value);
  }

  void Acquire(const ScopedGeneric<int, TrackedIntTraits>& owner, int value) {
    auto it = owners->find(value);
    ASSERT_EQ(owners->end(), it);
    (*owners)[value] = &owner;
  }

  void Release(const ScopedGeneric<int, TrackedIntTraits>& owner, int value) {
    auto it = owners->find(value);
    ASSERT_NE(owners->end(), it);
    owners->erase(it);
  }

  std::unordered_set<int>* freed;
  OwnerMap* owners;
};

using ScopedTrackedInt = ScopedGeneric<int, TrackedIntTraits>;

}  // namespace

TEST(ScopedGenericTest, OwnershipTracking) {
  TrackedIntTraits::OwnerMap owners;
  std::unordered_set<int> freed;
  TrackedIntTraits traits(&freed, &owners);

#define ASSERT_OWNED(value, owner)            \
  ASSERT_TRUE(base::Contains(owners, value)); \
  ASSERT_EQ(&owner, owners[value]);           \
  ASSERT_FALSE(base::Contains(freed, value))

#define ASSERT_UNOWNED(value)                  \
  ASSERT_FALSE(base::Contains(owners, value)); \
  ASSERT_FALSE(base::Contains(freed, value))

#define ASSERT_FREED(value)                    \
  ASSERT_FALSE(base::Contains(owners, value)); \
  ASSERT_TRUE(base::Contains(freed, value))

  // Constructor.
  {
    {
      ScopedTrackedInt a(0, traits);
      ASSERT_OWNED(0, a);
    }
    ASSERT_FREED(0);
  }

  owners.clear();
  freed.clear();

  // Reset.
  {
    ScopedTrackedInt a(0, traits);
    ASSERT_OWNED(0, a);
    a.reset(1);
    ASSERT_FREED(0);
    ASSERT_OWNED(1, a);
    a.reset();
    ASSERT_FREED(0);
    ASSERT_FREED(1);
  }

  owners.clear();
  freed.clear();

  // Release.
  {
    {
      ScopedTrackedInt a(0, traits);
      ASSERT_OWNED(0, a);
      int released = a.release();
      ASSERT_EQ(0, released);
      ASSERT_UNOWNED(0);
    }
    ASSERT_UNOWNED(0);
  }

  owners.clear();
  freed.clear();

  // Move constructor.
  {
    ScopedTrackedInt a(0, traits);
    ASSERT_OWNED(0, a);
    {
      ScopedTrackedInt b(std::move(a));
      ASSERT_OWNED(0, b);
    }
    ASSERT_FREED(0);
  }

  owners.clear();
  freed.clear();

  // Move assignment.
  {
    {
      ScopedTrackedInt a(0, traits);
      ScopedTrackedInt b(1, traits);
      ASSERT_OWNED(0, a);
      ASSERT_OWNED(1, b);
      a = std::move(b);
      ASSERT_OWNED(1, a);
      ASSERT_FREED(0);
    }
    ASSERT_FREED(1);
  }

  owners.clear();
  freed.clear();

  // Swap.
  {
    {
      ScopedTrackedInt a(0, traits);
      ScopedTrackedInt b(1, traits);
      ASSERT_OWNED(0, a);
      ASSERT_OWNED(1, b);
      a.swap(b);
      ASSERT_OWNED(1, a);
      ASSERT_OWNED(0, b);
    }
    ASSERT_FREED(0);
    ASSERT_FREED(1);
  }

  owners.clear();
  freed.clear();

#undef ASSERT_OWNED
#undef ASSERT_UNOWNED
#undef ASSERT_FREED
}

// Cheesy manual "no compile" test for manually validating changes.
#if 0
TEST(ScopedGenericTest, NoCompile) {
  // Assignment shouldn't work.
  /*{
    ScopedInt a(kFirst, traits);
    ScopedInt b(a);
  }*/

  // Comparison shouldn't work.
  /*{
    ScopedInt a(kFirst, traits);
    ScopedInt b(kFirst, traits);
    if (a == b) {
    }
  }*/

  // Implicit conversion to bool shouldn't work.
  /*{
    ScopedInt a(kFirst, traits);
    bool result = a;
  }*/
}
#endif

}  // namespace base
