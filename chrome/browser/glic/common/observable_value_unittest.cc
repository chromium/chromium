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

class TestReceiver {
 public:
  void OnValueChanged(const int& val) { values_.push_back(val); }
  void OnNotify() { ++notify_count_; }

  const std::vector<int>& values() const { return values_; }
  int notify_count() const { return notify_count_; }

 private:
  std::vector<int> values_;
  int notify_count_ = 0;
};

class ConstTestReceiver {
 public:
  void OnValueChanged(const int& value) const { values_.push_back(value); }
  void OnNotify() const { ++notify_count_; }

  const std::vector<int>& values() const { return values_; }
  int notify_count() const { return notify_count_; }

 private:
  mutable std::vector<int> values_;
  mutable int notify_count_ = 0;
};

TEST(ObservableValueTest, AddObserverMemberFunctionWithArg) {
  ObservableValue<int> obs(10);
  TestReceiver receiver;
  base::CallbackListSubscription sub =
      obs.AddObserver(&receiver, &TestReceiver::OnValueChanged);

  obs.Set(20);
  EXPECT_THAT(receiver.values(), testing::ElementsAre(20));
}

TEST(ObservableValueTest, AddObserverMemberFunctionNoArg) {
  ObservableValue<int> obs(10);
  TestReceiver receiver;
  base::CallbackListSubscription sub =
      obs.AddObserver(&receiver, &TestReceiver::OnNotify);

  obs.Set(20);
  EXPECT_EQ(1, receiver.notify_count());
}

TEST(ObservableValueTest, AddObserverAndNotifyMemberFunctionWithArg) {
  ObservableValue<int> obs(10);
  TestReceiver receiver;
  base::CallbackListSubscription sub =
      obs.AddObserverAndNotify(&receiver, &TestReceiver::OnValueChanged);

  EXPECT_THAT(receiver.values(), testing::ElementsAre(10));
  obs.Set(20);
  EXPECT_THAT(receiver.values(), testing::ElementsAre(10, 20));
}

TEST(ObservableValueTest, AddObserverAndNotifyMemberFunctionNoArg) {
  ObservableValue<int> obs(10);
  TestReceiver receiver;
  base::CallbackListSubscription sub =
      obs.AddObserverAndNotify(&receiver, &TestReceiver::OnNotify);

  EXPECT_EQ(1, receiver.notify_count());
  obs.Set(20);
  EXPECT_EQ(2, receiver.notify_count());
}

TEST(ObservableValueTest, ConstMemberFunctionAddObserver) {
  ObservableValue<int> obs(10);
  const ConstTestReceiver receiver;
  base::CallbackListSubscription sub_arg =
      obs.AddObserver(&receiver, &ConstTestReceiver::OnValueChanged);
  base::CallbackListSubscription sub_no_arg =
      obs.AddObserver(&receiver, &ConstTestReceiver::OnNotify);

  obs.Set(20);
  EXPECT_THAT(receiver.values(), testing::ElementsAre(20));
  EXPECT_EQ(1, receiver.notify_count());
}

TEST(ObservableValueTest, ConstMemberFunctionAddObserverAndNotify) {
  ObservableValue<int> obs(10);
  const ConstTestReceiver receiver;
  base::CallbackListSubscription sub_arg =
      obs.AddObserverAndNotify(&receiver, &ConstTestReceiver::OnValueChanged);
  base::CallbackListSubscription sub_no_arg =
      obs.AddObserverAndNotify(&receiver, &ConstTestReceiver::OnNotify);

  EXPECT_THAT(receiver.values(), testing::ElementsAre(10));
  EXPECT_EQ(1, receiver.notify_count());
  obs.Set(20);
  EXPECT_THAT(receiver.values(), testing::ElementsAre(10, 20));
  EXPECT_EQ(2, receiver.notify_count());
}

TEST(ObservableValueTest, ConstMemberFunctionObserve) {
  ObservableValue<int> obs(10);
  const ConstTestReceiver receiver;
  ObservedValue<int> observed =
      obs.Observe(&receiver, &ConstTestReceiver::OnNotify);

  EXPECT_EQ(0, receiver.notify_count());
  obs.Set(20);
  EXPECT_EQ(1, receiver.notify_count());
}

TEST(ObservableValueTest, ObserveAndNotify) {
  ObservableValue<int> obs(10);
  TestReceiver receiver;
  ObservedValue<int> observed =
      obs.ObserveAndNotify(&receiver, &TestReceiver::OnNotify);

  EXPECT_EQ(1, receiver.notify_count());
  obs.Set(20);
  EXPECT_EQ(2, receiver.notify_count());

  const ConstTestReceiver const_receiver;
  ObservedValue<int> const_observed =
      obs.ObserveAndNotify(&const_receiver, &ConstTestReceiver::OnNotify);

  EXPECT_EQ(1, const_receiver.notify_count());
  obs.Set(30);
  EXPECT_EQ(2, const_receiver.notify_count());
  EXPECT_EQ(3, receiver.notify_count());
}

TEST(ObservableValueTest, ObservedValueBasicAndAccessors) {
  struct Data {
    int id;
    std::string text;
  };
  ObservableValue<Data> obs(Data{.id = 1, .text = "hello"});
  TestReceiver receiver;
  ObservedValue<Data> observed(obs, &receiver, &TestReceiver::OnNotify);

  EXPECT_EQ(1, observed.get().id);
  EXPECT_EQ("hello", observed.value().text);
  EXPECT_EQ(1, (*observed).id);
  EXPECT_EQ("hello", observed->text);

  obs.Set(Data{.id = 2, .text = "world"});
  EXPECT_EQ(1, receiver.notify_count());
  EXPECT_EQ(2, observed->id);
  EXPECT_EQ("world", observed->text);
}

TEST(ObservableValueTest, ObservedValueLifecycleAndMove) {
  ObservableValue<int> obs(10);
  TestReceiver receiver;

  {
    ObservedValue<int> observed =
        obs.Observe(&receiver, &TestReceiver::OnNotify);
    EXPECT_TRUE(obs.HasObservers());
    obs.Set(20);
    EXPECT_EQ(1, receiver.notify_count());

    ObservedValue<int> moved = std::move(observed);
    obs.Set(30);
    EXPECT_EQ(2, receiver.notify_count());
  }

  EXPECT_FALSE(obs.HasObservers());
  obs.Set(40);
  EXPECT_EQ(2, receiver.notify_count());
}

TEST(ObservableValueTest, OptionalObservedValueEmplace) {
  ObservableValue<int> obs(10);
  TestReceiver receiver;
  std::optional<ObservedValue<int>> opt_observed;

  EXPECT_FALSE(opt_observed.has_value());
  EXPECT_FALSE(obs.HasObservers());

  opt_observed.emplace(obs.Observe(&receiver, &TestReceiver::OnNotify));
  EXPECT_TRUE(opt_observed.has_value());
  EXPECT_TRUE(obs.HasObservers());
  EXPECT_EQ(10, opt_observed->get());

  obs.Set(20);
  EXPECT_EQ(1, receiver.notify_count());
  EXPECT_EQ(20, (*opt_observed).value());

  opt_observed.reset();
  EXPECT_FALSE(obs.HasObservers());
  obs.Set(30);
  EXPECT_EQ(1, receiver.notify_count());
}

enum class TestColor { kRed, kGreen, kBlue };

TEST(ObservableValueTest, EqualityOperators) {
  ObservableValue<int> a(42);
  ObservableValue<int> b(42);
  ObservableValue<int> c(100);

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_EQ(a, 42);
  EXPECT_EQ(42, a);
  EXPECT_NE(a, 100);
  EXPECT_NE(100, a);

  TestReceiver receiver;
  ObservedValue<int> obs_a = a.Observe(&receiver, &TestReceiver::OnNotify);
  ObservedValue<int> obs_b = b.Observe(&receiver, &TestReceiver::OnNotify);
  ObservedValue<int> obs_c = c.Observe(&receiver, &TestReceiver::OnNotify);

  EXPECT_EQ(obs_a, obs_b);
  EXPECT_NE(obs_a, obs_c);
  EXPECT_EQ(obs_a, 42);
  EXPECT_EQ(42, obs_a);
  EXPECT_NE(obs_a, 100);
  EXPECT_NE(100, obs_a);

  EXPECT_EQ(obs_a, a);
  EXPECT_EQ(a, obs_a);
  EXPECT_NE(obs_a, c);
  EXPECT_NE(c, obs_a);
}

TEST(ObservableValueTest, EqualityWithOptionalAndEnum) {
  ObservableValue<std::optional<int>> opt_a(42);
  ObservableValue<std::optional<int>> opt_none(std::nullopt);
  TestReceiver receiver;
  ObservedValue<std::optional<int>> obs_opt_a =
      opt_a.Observe(&receiver, &TestReceiver::OnNotify);
  ObservedValue<std::optional<int>> obs_opt_none =
      opt_none.Observe(&receiver, &TestReceiver::OnNotify);

  EXPECT_EQ(opt_a, 42);
  EXPECT_EQ(42, opt_a);
  EXPECT_NE(opt_a, 99);
  EXPECT_NE(opt_a, std::nullopt);
  EXPECT_EQ(opt_none, std::nullopt);
  EXPECT_EQ(std::nullopt, opt_none);
  EXPECT_NE(opt_none, 42);

  EXPECT_EQ(obs_opt_a, 42);
  EXPECT_EQ(42, obs_opt_a);
  EXPECT_NE(obs_opt_a, 99);
  EXPECT_NE(obs_opt_a, std::nullopt);
  EXPECT_EQ(obs_opt_none, std::nullopt);
  EXPECT_EQ(std::nullopt, obs_opt_none);
  EXPECT_NE(obs_opt_none, 42);

  ObservableValue<TestColor> color(TestColor::kRed);
  ObservedValue<TestColor> obs_color =
      color.Observe(&receiver, &TestReceiver::OnNotify);

  EXPECT_EQ(color, TestColor::kRed);
  EXPECT_NE(color, TestColor::kBlue);
  EXPECT_EQ(obs_color, TestColor::kRed);
  EXPECT_NE(obs_color, TestColor::kBlue);
}

}  // namespace
}  // namespace glic
