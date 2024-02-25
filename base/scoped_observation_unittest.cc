// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_observation.h"

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_observation_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class TestSourceObserver {
 public:
  virtual ~TestSourceObserver() = default;
};

class TestSource {
 public:
  void AddObserver(TestSourceObserver* observer);
  void RemoveObserver(TestSourceObserver* observer);

  bool HasObserver(TestSourceObserver* observer) const;
  size_t num_observers() const { return observers_.size(); }

 private:
  std::vector<raw_ptr<TestSourceObserver, VectorExperimental>> observers_;
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
    const TestScopedObservation& cobs = obs;
    EXPECT_EQ(0u, s1.num_observers());
    EXPECT_FALSE(s1.HasObserver(&o1));
    EXPECT_EQ(obs.GetSource(), nullptr);
    EXPECT_EQ(cobs.GetSource(), nullptr);

    obs.Observe(&s1);
    EXPECT_EQ(1u, s1.num_observers());
    EXPECT_TRUE(s1.HasObserver(&o1));
    TestSource* const got_source = obs.GetSource();
    EXPECT_EQ(got_source, &s1);
    EXPECT_EQ(cobs.GetSource(), &s1);
  }

  // Test that the observation is removed when it goes out of scope.
  EXPECT_EQ(0u, s1.num_observers());
}

TEST(ScopedObservationTest, Reset) {
  TestSource s1;
  TestSourceObserver o1;
  TestScopedObservation obs(&o1);
  const TestScopedObservation& cobs = obs;
  EXPECT_EQ(0u, s1.num_observers());
  EXPECT_EQ(obs.GetSource(), nullptr);
  EXPECT_EQ(cobs.GetSource(), nullptr);
  obs.Reset();
  EXPECT_EQ(obs.GetSource(), nullptr);
  EXPECT_EQ(cobs.GetSource(), nullptr);

  obs.Observe(&s1);
  EXPECT_EQ(1u, s1.num_observers());
  EXPECT_TRUE(s1.HasObserver(&o1));
  EXPECT_EQ(obs.GetSource(), &s1);
  EXPECT_EQ(cobs.GetSource(), &s1);

  obs.Reset();
  EXPECT_EQ(0u, s1.num_observers());
  EXPECT_EQ(obs.GetSource(), nullptr);
  EXPECT_EQ(cobs.GetSource(), nullptr);

  // Safe to call with no observation.
  obs.Reset();
  EXPECT_EQ(0u, s1.num_observers());
  EXPECT_EQ(obs.GetSource(), nullptr);
  EXPECT_EQ(cobs.GetSource(), nullptr);
}

TEST(ScopedObservationTest, IsObserving) {
  TestSource s1;
  TestSourceObserver o1;
  TestScopedObservation obs(&o1);
  const TestScopedObservation& cobs = obs;
  EXPECT_FALSE(cobs.IsObserving());
  EXPECT_EQ(obs.GetSource(), nullptr);
  EXPECT_EQ(cobs.GetSource(), nullptr);

  obs.Observe(&s1);
  EXPECT_TRUE(cobs.IsObserving());
  EXPECT_EQ(obs.GetSource(), &s1);
  EXPECT_EQ(cobs.GetSource(), &s1);

  obs.Reset();
  EXPECT_FALSE(cobs.IsObserving());
  EXPECT_EQ(obs.GetSource(), nullptr);
  EXPECT_EQ(cobs.GetSource(), nullptr);
}

TEST(ScopedObservationTest, IsObservingSource) {
  TestSource s1;
  TestSource s2;
  TestSourceObserver o1;
  TestScopedObservation obs(&o1);
  const TestScopedObservation& cobs = obs;
  EXPECT_FALSE(cobs.IsObservingSource(&s1));
  EXPECT_FALSE(cobs.IsObservingSource(&s2));
  EXPECT_EQ(obs.GetSource(), nullptr);
  EXPECT_EQ(cobs.GetSource(), nullptr);

  obs.Observe(&s1);
  EXPECT_TRUE(cobs.IsObservingSource(&s1));
  EXPECT_FALSE(cobs.IsObservingSource(&s2));
  EXPECT_EQ(obs.GetSource(), &s1);
  EXPECT_EQ(cobs.GetSource(), &s1);

  obs.Reset();
  EXPECT_FALSE(cobs.IsObservingSource(&s1));
  EXPECT_FALSE(cobs.IsObservingSource(&s2));
  EXPECT_EQ(obs.GetSource(), nullptr);
  EXPECT_EQ(cobs.GetSource(), nullptr);
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
    ScopedObservation<TestSourceWithNonDefaultNames, TestSourceObserver>;

}  // namespace

template <>
struct ScopedObservationTraits<TestSourceWithNonDefaultNames,
                               TestSourceObserver> {
  static void AddObserver(TestSourceWithNonDefaultNames* source,
                          TestSourceObserver* observer) {
    source->AddFoo(observer);
  }
  static void RemoveObserver(TestSourceWithNonDefaultNames* source,
                             TestSourceObserver* observer) {
    source->RemoveFoo(observer);
  }
};

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

  EXPECT_EQ(0u, s1.impl().num_observers());
}

namespace {

// A forward-declared test source.

class TestSourceFwd;

class ObservationHolder : public TestSourceObserver {
 public:
  // Declared but not defined since TestSourceFwd is not yet defined.
  explicit ObservationHolder(TestSourceFwd* source);

 private:
  // ScopedObservation<> is instantiated with a forward-declared parameter.
  ScopedObservation<TestSourceFwd, TestSourceObserver> obs_{this};
};

// TestSourceFwd gets an actual definition!
class TestSourceFwd : public TestSource {};

// Calling ScopedObservation::Observe() requires an actual definition rather
// than just a forward declaration; make sure it compiles now that there is a
// definition.
ObservationHolder::ObservationHolder(TestSourceFwd* source) {
  obs_.Observe(source);
}

}  // namespace

TEST(ScopedObservationTest, ForwardDeclaredSource) {
  TestSourceFwd s;
  ASSERT_EQ(s.num_observers(), 0U);
  {
    ObservationHolder o(&s);
    ASSERT_EQ(s.num_observers(), 1U);
  }
  ASSERT_EQ(s.num_observers(), 0U);
}

namespace {

class TestSourceWithNonDefaultNamesFwd;

class ObservationWithNonDefaultNamesHolder : public TestSourceObserver {
 public:
  // Declared but not defined since TestSourceWithNonDefaultNamesFwd is not yet
  // defined.
  explicit ObservationWithNonDefaultNamesHolder(
      TestSourceWithNonDefaultNamesFwd* source);

 private:
  // ScopedObservation<> is instantiated with a forward-declared parameter.
  ScopedObservation<TestSourceWithNonDefaultNamesFwd, TestSourceObserver> obs_{
      this};
};

// TestSourceWithNonDefaultNamesFwd gets an actual definition!
class TestSourceWithNonDefaultNamesFwd : public TestSourceWithNonDefaultNames {
};

}  // namespace

// Now we define the corresponding traits. ScopedObservationTraits
// specializations must be defined in base::, since that is where the primary
// template definition lives.
template <>
struct ScopedObservationTraits<TestSourceWithNonDefaultNamesFwd,
                               TestSourceObserver> {
  static void AddObserver(TestSourceWithNonDefaultNamesFwd* source,
                          TestSourceObserver* observer) {
    source->AddFoo(observer);
  }
  static void RemoveObserver(TestSourceWithNonDefaultNamesFwd* source,
                             TestSourceObserver* observer) {
    source->RemoveFoo(observer);
  }
};

namespace {

// Calling ScopedObservation::Observe() requires an actual definition rather
// than just a forward declaration; make sure it compiles now that there is
// a definition.
ObservationWithNonDefaultNamesHolder::ObservationWithNonDefaultNamesHolder(
    TestSourceWithNonDefaultNamesFwd* source) {
  obs_.Observe(source);
}

}  // namespace

TEST(ScopedObservationTest, ForwardDeclaredSourceWithNonDefaultNames) {
  TestSourceWithNonDefaultNamesFwd s;
  ASSERT_EQ(s.impl().num_observers(), 0U);
  {
    ObservationWithNonDefaultNamesHolder o(&s);
    ASSERT_EQ(s.impl().num_observers(), 1U);
  }
  ASSERT_EQ(s.impl().num_observers(), 0U);
}

}  // namespace base
