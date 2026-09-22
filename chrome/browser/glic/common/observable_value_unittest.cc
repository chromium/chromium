// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/observable_value.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

TEST(ObservableValueTest, InitialValueAndAccessors) {
  ObservableValue<int> obs(42);
  EXPECT_EQ(42, obs.get());
  EXPECT_EQ(42, obs.value());
  EXPECT_EQ(42, *obs);
}

TEST(ObservableValueTest, DefaultConstructor) {
  ObservableValue<int> obs_int;
  EXPECT_EQ(0, obs_int.get());

  ObservableValue<std::string> obs_str;
  EXPECT_EQ("", obs_str.get());
}

TEST(ObservableValueTest, PointerAccess) {
  struct Data {
    std::string name;
  };
  ObservableValue<Data> obs(Data{.name = "test"});
  EXPECT_EQ("test", obs->name);
}

TEST(ObservableValueTest, SetNotifiesOnChange) {
  ObservableValue<int> obs(10);
  std::vector<int> observed;

  base::CallbackListSubscription sub =
      obs.AddObserver(base::BindLambdaForTesting(
          [&](const int& val) { observed.push_back(val); }));

  // Value change should notify.
  obs.Set(20);
  EXPECT_EQ(20, obs.get());
  EXPECT_THAT(observed, testing::ElementsAre(20));

  // Same value should NOT notify.
  obs.Set(20);
  EXPECT_THAT(observed, testing::ElementsAre(20));

  // Another change should notify.
  obs.Set(30);
  EXPECT_EQ(30, obs.get());
  EXPECT_THAT(observed, testing::ElementsAre(20, 30));
}

TEST(ObservableValueTest, NotifyForcesNotification) {
  ObservableValue<int> obs(10);
  int call_count = 0;

  base::CallbackListSubscription sub = obs.AddObserver(
      base::BindLambdaForTesting([&](const int&) { ++call_count; }));

  obs.Notify();
  EXPECT_EQ(1, call_count);
}

TEST(ObservableValueTest, AddObserverAndNotify) {
  ObservableValue<int> obs(10);
  std::vector<int> observed;

  base::CallbackListSubscription sub =
      obs.AddObserverAndNotify(base::BindLambdaForTesting(
          [&](const int& val) { observed.push_back(val); }));

  // Should have been notified immediately with initial value.
  EXPECT_THAT(observed, testing::ElementsAre(10));

  obs.Set(20);
  EXPECT_THAT(observed, testing::ElementsAre(10, 20));
}

TEST(ObservableValueTest, ClosureObserver) {
  ObservableValue<int> obs(10);
  int call_count = 0;

  base::CallbackListSubscription sub =
      obs.AddObserver(base::BindLambdaForTesting([&]() { ++call_count; }));

  obs.Set(20);
  EXPECT_EQ(1, call_count);
}

TEST(ObservableValueTest, ClosureObserverAndNotify) {
  ObservableValue<int> obs(10);
  int call_count = 0;

  base::CallbackListSubscription sub = obs.AddObserverAndNotify(
      base::BindLambdaForTesting([&]() { ++call_count; }));

  EXPECT_EQ(1, call_count);
  obs.Set(20);
  EXPECT_EQ(2, call_count);
}

TEST(ObservableValueTest, SubscriptionLifecycle) {
  ObservableValue<int> obs(10);
  int call_count = 0;

  {
    base::CallbackListSubscription sub = obs.AddObserver(
        base::BindLambdaForTesting([&](const int&) { ++call_count; }));
    EXPECT_TRUE(obs.HasObservers());
    obs.Set(20);
    EXPECT_EQ(1, call_count);
  }

  // Subscription destroyed; further changes should not notify.
  EXPECT_FALSE(obs.HasObservers());
  obs.Set(30);
  EXPECT_EQ(1, call_count);
}

TEST(ObservableValueTest, NonEqualityComparableType) {
  struct NonComparable {
    int x;
  };

  ObservableValue<NonComparable> obs(NonComparable{.x = 1});
  int call_count = 0;

  base::CallbackListSubscription sub = obs.AddObserver(
      base::BindLambdaForTesting([&](const NonComparable&) { ++call_count; }));

  obs.Set(NonComparable{.x = 1});
  EXPECT_EQ(1, call_count);
}

TEST(ObservableValueTest, ObservableValueView) {
  ObservableValue<int> obs(10);
  ObservableValueView<int>& view = obs;
  EXPECT_EQ(10, view.get());
  EXPECT_EQ(10, *view);

  int call_count = 0;
  base::CallbackListSubscription sub = view.AddObserver(
      base::BindLambdaForTesting([&](const int& val) { ++call_count; }));

  obs.Set(20);
  EXPECT_EQ(1, call_count);
  EXPECT_EQ(20, view.get());
}

}  // namespace
}  // namespace glic
