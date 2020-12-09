// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_observation.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class TestSourceObserver {};

class TestSource {
 public:
  void AddObserver(TestSourceObserver* observer);
  void RemoveObserver(TestSourceObserver* observer);

  bool HasObserver(TestSourceObserver* observer) const;
  size_t num_observers() const { return observers_.size(); }

 private:
  std::vector<TestSourceObserver*> observers_;
};

void TestSource::AddObserver(TestSourceObserver* observer) {
  observers_.push_back(observer);
}

void TestSource::RemoveObserver(TestSourceObserver* observer) {
  auto it = base::ranges::find(observers_, observer);
  EXPECT_TRUE(it != observers_.end());
  observers_.erase(it);
}

bool TestSource::HasObserver(TestSourceObserver* observer) const {
  return base::Contains(observers_, observer);
}

using TestScopedObservation = ScopedObservation<TestSource, TestSourceObserver>;

}  // namespace

TEST(ScopedObservationTest, RemovesObservationOnDestruction) {
  TestSource s1;

  {
    TestSourceObserver o1;
    TestScopedObservation obs(&o1);
    EXPECT_EQ(0u, s1.num_observers());
    EXPECT_FALSE(s1.HasObserver(&o1));

    obs.Observe(&s1);
    EXPECT_EQ(1u, s1.num_observers());
    EXPECT_TRUE(s1.HasObserver(&o1));
  }

  // Test that the observation is removed when it goes out of scope.
  EXPECT_EQ(0u, s1.num_observers());
}

TEST(ScopedObservationTest, Reset) {
  TestSource s1;
  TestSourceObserver o1;
  TestScopedObservation obs(&o1);
  EXPECT_EQ(0u, s1.num_observers());
  obs.Reset();

  obs.Observe(&s1);
  EXPECT_EQ(1u, s1.num_observers());
  EXPECT_TRUE(s1.HasObserver(&o1));

  obs.Reset();
  EXPECT_EQ(0u, s1.num_observers());

  // Safe to call with no observation.
  obs.Reset();
  EXPECT_EQ(0u, s1.num_observers());
}

TEST(ScopedObservationTest, IsObserving) {
  TestSource s1;
  TestSourceObserver o1;
  TestScopedObservation obs(&o1);
  EXPECT_FALSE(obs.IsObserving());

  obs.Observe(&s1);
  EXPECT_TRUE(obs.IsObserving());

  obs.Reset();
  EXPECT_FALSE(obs.IsObserving());
}

TEST(ScopedObservationTest, IsObservingSource) {
  TestSource s1;
  TestSource s2;
  TestSourceObserver o1;
  TestScopedObservation obs(&o1);
  EXPECT_FALSE(obs.IsObservingSource(&s1));
  EXPECT_FALSE(obs.IsObservingSource(&s2));

  obs.Observe(&s1);
  EXPECT_TRUE(obs.IsObservingSource(&s1));
  EXPECT_FALSE(obs.IsObservingSource(&s2));

  obs.Reset();
  EXPECT_FALSE(obs.IsObservingSource(&s1));
  EXPECT_FALSE(obs.IsObservingSource(&s2));
}

namespace {

// A test source with oddly named Add/Remove functions.
class TestSourceWithNonDefaultNames {
 public:
  void AddFoo(TestSourceObserver* observer) { impl_.AddObserver(observer); }
  void RemoveFoo(TestSourceObserver* observer) {
    impl_.RemoveObserver(observer);
  }

  const TestSource& impl() const { return impl_; }

 private:
  TestSource impl_;
};

using TestScopedObservationWithNonDefaultNames =
    ScopedObservation<TestSourceWithNonDefaultNames,
                      TestSourceObserver,
                      &TestSourceWithNonDefaultNames::AddFoo,
                      &TestSourceWithNonDefaultNames::RemoveFoo>;

}  // namespace

TEST(ScopedObservationTest, NonDefaultNames) {
  TestSourceWithNonDefaultNames s1;
  TestSourceObserver o1;

  EXPECT_EQ(0u, s1.impl().num_observers());
  {
    TestScopedObservationWithNonDefaultNames obs(&o1);
    obs.Observe(&s1);
    EXPECT_EQ(1u, s1.impl().num_observers());
    EXPECT_TRUE(s1.impl().HasObserver(&o1));
  }
}

}  // namespace base
