// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class Listener {
 public:
  Listener() : total_(0), scaler_(1) {}
  explicit Listener(int scaler) : total_(0), scaler_(scaler) {}
  void IncrementTotal() { total_++; }
  void IncrementByMultipleOfScaler(int x) { total_ += x * scaler_; }

  int total() const { return total_; }

 private:
  int total_;
  int scaler_;
  DISALLOW_COPY_AND_ASSIGN(Listener);
};

class Remover {
 public:
  Remover() : total_(0) {}
  void IncrementTotalAndRemove() {
    total_++;
    removal_subscription_.reset();
  }
  void SetSubscriptionToRemove(
      std::unique_ptr<CallbackList<void(void)>::Subscription> sub) {
    removal_subscription_ = std::move(sub);
  }

  int total() const { return total_; }

 private:
  int total_;
  std::unique_ptr<CallbackList<void(void)>::Subscription> removal_subscription_;
  DISALLOW_COPY_AND_ASSIGN(Remover);
};

class Adder {
 public:
  explicit Adder(CallbackList<void(void)>* cb_reg)
      : added_(false),
        total_(0),
        cb_reg_(cb_reg) {
  }
  void AddCallback() {
    if (!added_) {
      added_ = true;
      subscription_ =
          cb_reg_->Add(BindRepeating(&Adder::IncrementTotal, Unretained(this)));
    }
  }
  void IncrementTotal() { total_++; }

  bool added() const { return added_; }

  int total() const { return total_; }

 private:
  bool added_;
  int total_;
  CallbackList<void(void)>* cb_reg_;
  std::unique_ptr<CallbackList<void(void)>::Subscription> subscription_;
  DISALLOW_COPY_AND_ASSIGN(Adder);
};

class Summer {
 public:
  Summer() : value_(0) {}

  void AddOneParam(int a) { value_ = a; }
  void AddTwoParam(int a, int b) { value_ = a + b; }
  void AddThreeParam(int a, int b, int c) { value_ = a + b + c; }
  void AddFourParam(int a, int b, int c, int d) { value_ = a + b + c + d; }
  void AddFiveParam(int a, int b, int c, int d, int e) {
    value_ = a + b + c + d + e;
  }
  void AddSixParam(int a, int b, int c, int d, int e , int f) {
    value_ = a + b + c + d + e + f;
  }

  int value() const { return value_; }

 private:
  int value_;
  DISALLOW_COPY_AND_ASSIGN(Summer);
};

class Counter {
 public:
  Counter() : value_(0) {}

  void Increment() { value_++; }

  int value() const { return value_; }

 private:
  int value_;
  DISALLOW_COPY_AND_ASSIGN(Counter);
};

// Sanity check that we can instantiate a CallbackList for each arity.
TEST(CallbackListTest, ArityTest) {
  Summer s;

  CallbackList<void(int)> c1;
  std::unique_ptr<CallbackList<void(int)>::Subscription> subscription1 =
      c1.Add(BindRepeating(&Summer::AddOneParam, Unretained(&s)));

  c1.Notify(1);
  EXPECT_EQ(1, s.value());

  CallbackList<void(int, int)> c2;
  std::unique_ptr<CallbackList<void(int, int)>::Subscription> subscription2 =
      c2.Add(BindRepeating(&Summer::AddTwoParam, Unretained(&s)));

  c2.Notify(1, 2);
  EXPECT_EQ(3, s.value());

  CallbackList<void(int, int, int)> c3;
  std::unique_ptr<CallbackList<void(int, int, int)>::Subscription>
      subscription3 =
          c3.Add(BindRepeating(&Summer::AddThreeParam, Unretained(&s)));

  c3.Notify(1, 2, 3);
  EXPECT_EQ(6, s.value());

  CallbackList<void(int, int, int, int)> c4;
  std::unique_ptr<CallbackList<void(int, int, int, int)>::Subscription>
      subscription4 =
          c4.Add(BindRepeating(&Summer::AddFourParam, Unretained(&s)));

  c4.Notify(1, 2, 3, 4);
  EXPECT_EQ(10, s.value());

  CallbackList<void(int, int, int, int, int)> c5;
  std::unique_ptr<CallbackList<void(int, int, int, int, int)>::Subscription>
      subscription5 =
          c5.Add(BindRepeating(&Summer::AddFiveParam, Unretained(&s)));

  c5.Notify(1, 2, 3, 4, 5);
  EXPECT_EQ(15, s.value());

  CallbackList<void(int, int, int, int, int, int)> c6;
  std::unique_ptr<
      CallbackList<void(int, int, int, int, int, int)>::Subscription>
      subscription6 =
          c6.Add(BindRepeating(&Summer::AddSixParam, Unretained(&s)));

  c6.Notify(1, 2, 3, 4, 5, 6);
  EXPECT_EQ(21, s.value());
}

// Sanity check that closures added to the list will be run, and those removed
// from the list will not be run.
TEST(CallbackListTest, BasicTest) {
  CallbackList<void(void)> cb_reg;
  Listener a, b, c;

  std::unique_ptr<CallbackList<void(void)>::Subscription> a_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&a)));
  std::unique_ptr<CallbackList<void(void)>::Subscription> b_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&b)));

  EXPECT_TRUE(a_subscription.get());
  EXPECT_TRUE(b_subscription.get());

  cb_reg.Notify();

  EXPECT_EQ(1, a.total());
  EXPECT_EQ(1, b.total());

  b_subscription.reset();

  std::unique_ptr<CallbackList<void(void)>::Subscription> c_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&c)));

  cb_reg.Notify();

  EXPECT_EQ(2, a.total());
  EXPECT_EQ(1, b.total());
  EXPECT_EQ(1, c.total());

  a_subscription.reset();
  b_subscription.reset();
  c_subscription.reset();
}

// Sanity check that callbacks with details added to the list will be run, with
// the correct details, and those removed from the list will not be run.
TEST(CallbackListTest, BasicTestWithParams) {
  CallbackList<void(int)> cb_reg;
  Listener a(1), b(-1), c(1);

  std::unique_ptr<CallbackList<void(int)>::Subscription> a_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementByMultipleOfScaler,
                               Unretained(&a)));
  std::unique_ptr<CallbackList<void(int)>::Subscription> b_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementByMultipleOfScaler,
                               Unretained(&b)));

  EXPECT_TRUE(a_subscription.get());
  EXPECT_TRUE(b_subscription.get());

  cb_reg.Notify(10);

  EXPECT_EQ(10, a.total());
  EXPECT_EQ(-10, b.total());

  b_subscription.reset();

  std::unique_ptr<CallbackList<void(int)>::Subscription> c_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementByMultipleOfScaler,
                               Unretained(&c)));

  cb_reg.Notify(10);

  EXPECT_EQ(20, a.total());
  EXPECT_EQ(-10, b.total());
  EXPECT_EQ(10, c.total());

  a_subscription.reset();
  b_subscription.reset();
  c_subscription.reset();
}

// Test the a callback can remove itself or a different callback from the list
// during iteration without invalidating the iterator.
TEST(CallbackListTest, RemoveCallbacksDuringIteration) {
  CallbackList<void(void)> cb_reg;
  Listener a, b;
  Remover remover_1, remover_2;

  std::unique_ptr<CallbackList<void(void)>::Subscription> remover_1_sub =
      cb_reg.Add(BindRepeating(&Remover::IncrementTotalAndRemove,
                               Unretained(&remover_1)));
  std::unique_ptr<CallbackList<void(void)>::Subscription> remover_2_sub =
      cb_reg.Add(BindRepeating(&Remover::IncrementTotalAndRemove,
                               Unretained(&remover_2)));
  std::unique_ptr<CallbackList<void(void)>::Subscription> a_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&a)));
  std::unique_ptr<CallbackList<void(void)>::Subscription> b_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&b)));

  // |remover_1| will remove itself.
  remover_1.SetSubscriptionToRemove(std::move(remover_1_sub));
  // |remover_2| will remove a.
  remover_2.SetSubscriptionToRemove(std::move(a_subscription));

  cb_reg.Notify();

  // |remover_1| runs once (and removes itself), |remover_2| runs once (and
  // removes a), |a| never runs, and |b| runs once.
  EXPECT_EQ(1, remover_1.total());
  EXPECT_EQ(1, remover_2.total());
  EXPECT_EQ(0, a.total());
  EXPECT_EQ(1, b.total());

  cb_reg.Notify();

  // Only |remover_2| and |b| run this time.
  EXPECT_EQ(1, remover_1.total());
  EXPECT_EQ(2, remover_2.total());
  EXPECT_EQ(0, a.total());
  EXPECT_EQ(2, b.total());
}

// Test that a callback can add another callback to the list durning iteration
// without invalidating the iterator. The newly added callback should be run on
// the current iteration as will all other callbacks in the list.
TEST(CallbackListTest, AddCallbacksDuringIteration) {
  CallbackList<void(void)> cb_reg;
  Adder a(&cb_reg);
  Listener b;
  std::unique_ptr<CallbackList<void(void)>::Subscription> a_subscription =
      cb_reg.Add(BindRepeating(&Adder::AddCallback, Unretained(&a)));
  std::unique_ptr<CallbackList<void(void)>::Subscription> b_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&b)));

  cb_reg.Notify();

  EXPECT_EQ(1, a.total());
  EXPECT_EQ(1, b.total());
  EXPECT_TRUE(a.added());

  cb_reg.Notify();

  EXPECT_EQ(2, a.total());
  EXPECT_EQ(2, b.total());
}

// Sanity check: notifying an empty list is a no-op.
TEST(CallbackListTest, EmptyList) {
  CallbackList<void(void)> cb_reg;

  cb_reg.Notify();
}

TEST(CallbackList, RemovalCallback) {
  Counter remove_count;
  CallbackList<void(void)> cb_reg;
  cb_reg.set_removal_callback(
      BindRepeating(&Counter::Increment, Unretained(&remove_count)));

  std::unique_ptr<CallbackList<void(void)>::Subscription> subscription =
      cb_reg.Add(DoNothing());

  // Removing a subscription outside of iteration signals the callback.
  EXPECT_EQ(0, remove_count.value());
  subscription.reset();
  EXPECT_EQ(1, remove_count.value());

  // Configure two subscriptions to remove themselves.
  Remover remover_1, remover_2;
  std::unique_ptr<CallbackList<void(void)>::Subscription> remover_1_sub =
      cb_reg.Add(BindRepeating(&Remover::IncrementTotalAndRemove,
                               Unretained(&remover_1)));
  std::unique_ptr<CallbackList<void(void)>::Subscription> remover_2_sub =
      cb_reg.Add(BindRepeating(&Remover::IncrementTotalAndRemove,
                               Unretained(&remover_2)));
  remover_1.SetSubscriptionToRemove(std::move(remover_1_sub));
  remover_2.SetSubscriptionToRemove(std::move(remover_2_sub));

  // The callback should be signaled exactly once.
  EXPECT_EQ(1, remove_count.value());
  cb_reg.Notify();
  EXPECT_EQ(2, remove_count.value());
  EXPECT_TRUE(cb_reg.empty());
}

TEST(CallbackList, AbandonSubscriptions) {
  Listener listener;
  std::unique_ptr<CallbackList<void(void)>::Subscription> subscription;
  {
    CallbackList<void(void)> cb_reg;
    subscription = cb_reg.Add(
        BindRepeating(&Listener::IncrementTotal, Unretained(&listener)));
    // Make sure the callback is signaled while cb_reg is in scope.
    cb_reg.Notify();
    // Exiting this scope and running the cb_reg destructor shouldn't fail.
  }
  EXPECT_EQ(1, listener.total());
  // The subscription from the destroyed callback list should be cancelled now.
  EXPECT_TRUE(subscription->IsCancelled());
}

}  // namespace
}  // namespace base
