// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_multi_source_observation.h"

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
  ASSERT_TRUE(it != observers_.end());
  observers_.erase(it);
}

bool TestSource::HasObserver(TestSourceObserver* observer) const {
  return base::Contains(observers_, observer);
}

using TestScopedMultiSourceObservation =
    ScopedMultiSourceObservation<TestSource, TestSourceObserver>;

class ScopedMultiSourceObservationTest : public testing::Test {
 public:
  TestSource* s1() { return &s1_; }
  TestSource* s2() { return &s2_; }
  TestSourceObserver* o1() { return &o1_; }

 private:
  TestSource s1_;
  TestSource s2_;
  TestSourceObserver o1_;
};

}  // namespace

TEST_F(ScopedMultiSourceObservationTest, RemovesSourcesOnDestruction) {
  {
    TestScopedMultiSourceObservation obs(o1());
    EXPECT_EQ(0u, s1()->num_observers());
    EXPECT_FALSE(s1()->HasObserver(o1()));

    obs.AddObservation(s1());
    EXPECT_EQ(1u, s1()->num_observers());
    EXPECT_TRUE(s1()->HasObserver(o1()));

    obs.AddObservation(s2());
    EXPECT_EQ(1u, s2()->num_observers());
    EXPECT_TRUE(s2()->HasObserver(o1()));
  }

  // Test that all observations are removed when it goes out of scope.
  EXPECT_EQ(0u, s1()->num_observers());
  EXPECT_EQ(0u, s2()->num_observers());
}

TEST_F(ScopedMultiSourceObservationTest, RemoveObservation) {
  TestScopedMultiSourceObservation obs(o1());
  EXPECT_EQ(0u, s1()->num_observers());
  EXPECT_FALSE(s1()->HasObserver(o1()));
  EXPECT_EQ(0u, s2()->num_observers());
  EXPECT_FALSE(s2()->HasObserver(o1()));

  obs.AddObservation(s1());
  EXPECT_EQ(1u, s1()->num_observers());
  EXPECT_TRUE(s1()->HasObserver(o1()));

  obs.AddObservation(s2());
  EXPECT_EQ(1u, s2()->num_observers());
  EXPECT_TRUE(s2()->HasObserver(o1()));

  obs.RemoveObservation(s1());
  EXPECT_EQ(0u, s1()->num_observers());
  EXPECT_FALSE(s1()->HasObserver(o1()));
  EXPECT_EQ(1u, s2()->num_observers());
  EXPECT_TRUE(s2()->HasObserver(o1()));

  obs.RemoveObservation(s2());
  EXPECT_EQ(0u, s1()->num_observers());
  EXPECT_FALSE(s1()->HasObserver(o1()));
  EXPECT_EQ(0u, s2()->num_observers());
  EXPECT_FALSE(s2()->HasObserver(o1()));
}

TEST_F(ScopedMultiSourceObservationTest, RemoveAllObservations) {
  TestScopedMultiSourceObservation obs(o1());
  EXPECT_EQ(0u, s1()->num_observers());
  EXPECT_FALSE(s1()->HasObserver(o1()));
  EXPECT_EQ(0u, s2()->num_observers());
  EXPECT_FALSE(s2()->HasObserver(o1()));

  obs.AddObservation(s1());
  obs.AddObservation(s2());
  EXPECT_EQ(1u, s1()->num_observers());
  EXPECT_TRUE(s1()->HasObserver(o1()));
  EXPECT_EQ(1u, s2()->num_observers());
  EXPECT_TRUE(s2()->HasObserver(o1()));

  obs.RemoveAllObservations();
  EXPECT_EQ(0u, s1()->num_observers());
  EXPECT_FALSE(s1()->HasObserver(o1()));
  EXPECT_EQ(0u, s2()->num_observers());
  EXPECT_FALSE(s2()->HasObserver(o1()));
}

TEST_F(ScopedMultiSourceObservationTest, IsObservingSource) {
  TestScopedMultiSourceObservation obs(o1());
  EXPECT_FALSE(obs.IsObservingSource(s1()));
  EXPECT_FALSE(obs.IsObservingSource(s2()));

  obs.AddObservation(s1());
  EXPECT_TRUE(obs.IsObservingSource(s1()));
  EXPECT_FALSE(obs.IsObservingSource(s2()));

  obs.AddObservation(s2());
  EXPECT_TRUE(obs.IsObservingSource(s1()));
  EXPECT_TRUE(obs.IsObservingSource(s2()));

  obs.RemoveObservation(s1());
  EXPECT_FALSE(obs.IsObservingSource(s1()));
  EXPECT_TRUE(obs.IsObservingSource(s2()));
}

TEST_F(ScopedMultiSourceObservationTest, IsObservingAnySource) {
  TestScopedMultiSourceObservation obs(o1());
  EXPECT_FALSE(obs.IsObservingAnySource());

  obs.AddObservation(s1());
  EXPECT_TRUE(obs.IsObservingAnySource());

  obs.AddObservation(s2());
  EXPECT_TRUE(obs.IsObservingAnySource());

  obs.RemoveAllObservations();
  EXPECT_FALSE(obs.IsObservingAnySource());
}

TEST_F(ScopedMultiSourceObservationTest, GetSourcesCount) {
  TestScopedMultiSourceObservation obs(o1());
  EXPECT_EQ(0u, obs.GetSourcesCount());

  obs.AddObservation(s1());
  EXPECT_EQ(1u, obs.GetSourcesCount());

  obs.AddObservation(s2());
  EXPECT_EQ(2u, obs.GetSourcesCount());

  obs.RemoveAllObservations();
  EXPECT_EQ(0u, obs.GetSourcesCount());
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

using TestScopedMultiSourceObservationWithNonDefaultNames =
    ScopedMultiSourceObservation<TestSourceWithNonDefaultNames,
                                 TestSourceObserver,
                                 &TestSourceWithNonDefaultNames::AddFoo,
                                 &TestSourceWithNonDefaultNames::RemoveFoo>;

}  // namespace

TEST_F(ScopedMultiSourceObservationTest, NonDefaultNames) {
  TestSourceWithNonDefaultNames nds1;

  EXPECT_EQ(0u, nds1.impl().num_observers());
  {
    TestScopedMultiSourceObservationWithNonDefaultNames obs(o1());
    obs.AddObservation(&nds1);
    EXPECT_EQ(1u, nds1.impl().num_observers());
    EXPECT_TRUE(nds1.impl().HasObserver(o1()));
  }

  EXPECT_EQ(0u, nds1.impl().num_observers());
}

}  // namespace base
