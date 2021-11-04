// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class Listener {
 public:
  Listener() = default;
  explicit Listener(int scaler) : scaler_(scaler) {}
  Listener(const Listener&) = delete;
  Listener& operator=(const Listener&) = delete;
  ~Listener() = default;

  void IncrementTotal() { ++total_; }

  void IncrementByMultipleOfScaler(int x) { total_ += x * scaler_; }

  int total() const { return total_; }

 private:
  int total_ = 0;
  int scaler_ = 1;
};

class Remover {
 public:
  Remover() = default;
  Remover(const Remover&) = delete;
  Remover& operator=(const Remover&) = delete;
  ~Remover() = default;

  void IncrementTotalAndRemove() {
    ++total_;
    removal_subscription_ = {};
  }

  void SetSubscriptionToRemove(CallbackListSubscription subscription) {
    removal_subscription_ = std::move(subscription);
  }

  int total() const { return total_; }

 private:
  int total_ = 0;
  CallbackListSubscription removal_subscription_;
};

class Adder {
 public:
  explicit Adder(RepeatingClosureList* cb_reg) : cb_reg_(cb_reg) {}
  Adder(const Adder&) = delete;
  Adder& operator=(const Adder&) = delete;
  ~Adder() = default;

  void AddCallback() {
    if (!added_) {
      added_ = true;
      subscription_ =
          cb_reg_->Add(BindRepeating(&Adder::IncrementTotal, Unretained(this)));
    }
  }

  void IncrementTotal() { ++total_; }

  bool added() const { return added_; }
  int total() const { return total_; }

 private:
  bool added_ = false;
  int total_ = 0;
  RepeatingClosureList* cb_reg_;
  CallbackListSubscription subscription_;
};

class Summer {
 public:
  Summer() = default;
  Summer(const Summer&) = delete;
  Summer& operator=(const Summer&) = delete;
  ~Summer() = default;

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
  int value_ = 0;
};

class Counter {
 public:
  Counter() = default;
  Counter(const Counter&) = delete;
  Counter& operator=(const Counter&) = delete;
  ~Counter() = default;

  void Increment() { ++value_; }

  int value() const { return value_; }

 private:
  int value_ = 0;
};

// Sanity check that we can instantiate a CallbackList for each arity.
TEST(CallbackListTest, ArityTest) {
  Summer s;

  RepeatingCallbackList<void(int)> c1;
  CallbackListSubscription subscription1 =
      c1.Add(BindRepeating(&Summer::AddOneParam, Unretained(&s)));

  c1.Notify(1);
  EXPECT_EQ(1, s.value());

  RepeatingCallbackList<void(int, int)> c2;
  CallbackListSubscription subscription2 =
      c2.Add(BindRepeating(&Summer::AddTwoParam, Unretained(&s)));

  c2.Notify(1, 2);
  EXPECT_EQ(3, s.value());

  RepeatingCallbackList<void(int, int, int)> c3;
  CallbackListSubscription subscription3 =
      c3.Add(BindRepeating(&Summer::AddThreeParam, Unretained(&s)));

  c3.Notify(1, 2, 3);
  EXPECT_EQ(6, s.value());

  RepeatingCallbackList<void(int, int, int, int)> c4;
  CallbackListSubscription subscription4 =
      c4.Add(BindRepeating(&Summer::AddFourParam, Unretained(&s)));

  c4.Notify(1, 2, 3, 4);
  EXPECT_EQ(10, s.value());

  RepeatingCallbackList<void(int, int, int, int, int)> c5;
  CallbackListSubscription subscription5 =
      c5.Add(BindRepeating(&Summer::AddFiveParam, Unretained(&s)));

  c5.Notify(1, 2, 3, 4, 5);
  EXPECT_EQ(15, s.value());

  RepeatingCallbackList<void(int, int, int, int, int, int)> c6;
  CallbackListSubscription subscription6 =
      c6.Add(BindRepeating(&Summer::AddSixParam, Unretained(&s)));

  c6.Notify(1, 2, 3, 4, 5, 6);
  EXPECT_EQ(21, s.value());
}

// Sanity check that closures added to the list will be run, and those removed
// from the list will not be run.
TEST(CallbackListTest, BasicTest) {
  Listener a, b, c;
  RepeatingClosureList cb_reg;

  CallbackListSubscription a_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&a)));
  CallbackListSubscription b_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&b)));
  cb_reg.AddUnsafe(BindRepeating(&Listener::IncrementTotal, Unretained(&c)));

  EXPECT_TRUE(a_subscription);
  EXPECT_TRUE(b_subscription);

  cb_reg.Notify();

  EXPECT_EQ(1, a.total());
  EXPECT_EQ(1, b.total());
  EXPECT_EQ(1, c.total());

  b_subscription = {};

  CallbackListSubscription c_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&c)));

  cb_reg.Notify();

  EXPECT_EQ(2, a.total());
  EXPECT_EQ(1, b.total());
  EXPECT_EQ(3, c.total());
}

// Similar to BasicTest but with OnceCallbacks instead of Repeating.
TEST(CallbackListTest, OnceCallbacks) {
  OnceClosureList cb_reg;
  Listener a, b, c;

  CallbackListSubscription a_subscription =
      cb_reg.Add(BindOnce(&Listener::IncrementTotal, Unretained(&a)));
  CallbackListSubscription b_subscription =
      cb_reg.Add(BindOnce(&Listener::IncrementTotal, Unretained(&b)));

  EXPECT_TRUE(a_subscription);
  EXPECT_TRUE(b_subscription);

  cb_reg.Notify();

  EXPECT_EQ(1, a.total());
  EXPECT_EQ(1, b.total());

  // OnceCallbacks should auto-remove themselves after calling Notify().
  EXPECT_TRUE(cb_reg.empty());

  // Destroying a subscription after the callback is canceled should not cause
  // any problems.
  b_subscription = {};

  CallbackListSubscription c_subscription =
      cb_reg.Add(BindOnce(&Listener::IncrementTotal, Unretained(&c)));

  cb_reg.Notify();

  EXPECT_EQ(1, a.total());
  EXPECT_EQ(1, b.total());
  EXPECT_EQ(1, c.total());
}

// Sanity check that callbacks with details added to the list will be run, with
// the correct details, and those removed from the list will not be run.
TEST(CallbackListTest, BasicTestWithParams) {
  using CallbackListType = RepeatingCallbackList<void(int)>;
  CallbackListType cb_reg;
  Listener a(1), b(-1), c(1);

  CallbackListSubscription a_subscription = cb_reg.Add(
      BindRepeating(&Listener::IncrementByMultipleOfScaler, Unretained(&a)));
  CallbackListSubscription b_subscription = cb_reg.Add(
      BindRepeating(&Listener::IncrementByMultipleOfScaler, Unretained(&b)));

  EXPECT_TRUE(a_subscription);
  EXPECT_TRUE(b_subscription);

  cb_reg.Notify(10);

  EXPECT_EQ(10, a.total());
  EXPECT_EQ(-10, b.total());

  b_subscription = {};

  CallbackListSubscription c_subscription = cb_reg.Add(
      BindRepeating(&Listener::IncrementByMultipleOfScaler, Unretained(&c)));

  cb_reg.Notify(10);

  EXPECT_EQ(20, a.total());
  EXPECT_EQ(-10, b.total());
  EXPECT_EQ(10, c.total());
}

// Test the a callback can remove itself or a different callback from the list
// during iteration without invalidating the iterator.
TEST(CallbackListTest, RemoveCallbacksDuringIteration) {
  RepeatingClosureList cb_reg;
  Listener a, b;
  Remover remover_1, remover_2;

  CallbackListSubscription remover_1_sub = cb_reg.Add(
      BindRepeating(&Remover::IncrementTotalAndRemove, Unretained(&remover_1)));
  CallbackListSubscription remover_2_sub = cb_reg.Add(
      BindRepeating(&Remover::IncrementTotalAndRemove, Unretained(&remover_2)));
  CallbackListSubscription a_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&a)));
  CallbackListSubscription b_subscription =
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

// Similar to RemoveCallbacksDuringIteration but with OnceCallbacks instead of
// Repeating.
TEST(CallbackListTest, RemoveOnceCallbacksDuringIteration) {
  OnceClosureList cb_reg;
  Listener a, b;
  Remover remover_1, remover_2;

  CallbackListSubscription remover_1_sub = cb_reg.Add(
      BindOnce(&Remover::IncrementTotalAndRemove, Unretained(&remover_1)));
  CallbackListSubscription remover_2_sub = cb_reg.Add(
      BindOnce(&Remover::IncrementTotalAndRemove, Unretained(&remover_2)));
  CallbackListSubscription a_subscription =
      cb_reg.Add(BindOnce(&Listener::IncrementTotal, Unretained(&a)));
  CallbackListSubscription b_subscription =
      cb_reg.Add(BindOnce(&Listener::IncrementTotal, Unretained(&b)));

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

  // Nothing runs this time.
  EXPECT_EQ(1, remover_1.total());
  EXPECT_EQ(1, remover_2.total());
  EXPECT_EQ(0, a.total());
  EXPECT_EQ(1, b.total());
}

// Test that a callback can add another callback to the list durning iteration
// without invalidating the iterator. The newly added callback should be run on
// the current iteration as will all other callbacks in the list.
TEST(CallbackListTest, AddCallbacksDuringIteration) {
  RepeatingClosureList cb_reg;
  Adder a(&cb_reg);
  Listener b;
  CallbackListSubscription a_subscription =
      cb_reg.Add(BindRepeating(&Adder::AddCallback, Unretained(&a)));
  CallbackListSubscription b_subscription =
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
  RepeatingClosureList cb_reg;

  cb_reg.Notify();
}

// empty() should be callable during iteration, and return false if not all the
// remaining callbacks in the list are null.
TEST(CallbackListTest, NonEmptyListDuringIteration) {
  // Declare items such that |cb_reg| is torn down before the subscriptions.
  // This ensures the removal callback's invariant that the callback list is
  // nonempty will always hold.
  Remover remover;
  Listener listener;
  CallbackListSubscription remover_sub, listener_sub;
  RepeatingClosureList cb_reg;
  cb_reg.set_removal_callback(base::BindRepeating(
      [](const RepeatingClosureList* callbacks) {
        EXPECT_FALSE(callbacks->empty());
      },
      Unretained(&cb_reg)));

  remover_sub = cb_reg.Add(
      BindRepeating(&Remover::IncrementTotalAndRemove, Unretained(&remover)));
  listener_sub = cb_reg.Add(
      BindRepeating(&Listener::IncrementTotal, Unretained(&listener)));

  // |remover| will remove |listener|.
  remover.SetSubscriptionToRemove(std::move(listener_sub));

  cb_reg.Notify();

  EXPECT_EQ(1, remover.total());
  EXPECT_EQ(0, listener.total());
}

// empty() should be callable during iteration, and return true if all the
// remaining callbacks in the list are null.
TEST(CallbackListTest, EmptyListDuringIteration) {
  OnceClosureList cb_reg;
  cb_reg.set_removal_callback(base::BindRepeating(
      [](const OnceClosureList* callbacks) { EXPECT_TRUE(callbacks->empty()); },
      Unretained(&cb_reg)));

  Remover remover;
  Listener listener;
  CallbackListSubscription remover_sub = cb_reg.Add(
      BindOnce(&Remover::IncrementTotalAndRemove, Unretained(&remover)));
  CallbackListSubscription listener_sub =
      cb_reg.Add(BindOnce(&Listener::IncrementTotal, Unretained(&listener)));

  // |remover| will remove |listener|.
  remover.SetSubscriptionToRemove(std::move(listener_sub));

  cb_reg.Notify();

  EXPECT_EQ(1, remover.total());
  EXPECT_EQ(0, listener.total());
}

TEST(CallbackListTest, RemovalCallback) {
  Counter remove_count;
  RepeatingClosureList cb_reg;
  cb_reg.set_removal_callback(
      BindRepeating(&Counter::Increment, Unretained(&remove_count)));

  CallbackListSubscription subscription = cb_reg.Add(DoNothing());

  // Removing a subscription outside of iteration signals the callback.
  EXPECT_EQ(0, remove_count.value());
  subscription = {};
  EXPECT_EQ(1, remove_count.value());

  // Configure two subscriptions to remove themselves.
  Remover remover_1, remover_2;
  CallbackListSubscription remover_1_sub = cb_reg.Add(
      BindRepeating(&Remover::IncrementTotalAndRemove, Unretained(&remover_1)));
  CallbackListSubscription remover_2_sub = cb_reg.Add(
      BindRepeating(&Remover::IncrementTotalAndRemove, Unretained(&remover_2)));
  remover_1.SetSubscriptionToRemove(std::move(remover_1_sub));
  remover_2.SetSubscriptionToRemove(std::move(remover_2_sub));

  // The callback should be signaled exactly once.
  EXPECT_EQ(1, remove_count.value());
  cb_reg.Notify();
  EXPECT_EQ(2, remove_count.value());
  EXPECT_TRUE(cb_reg.empty());
}

TEST(CallbackListTest, AbandonSubscriptions) {
  Listener listener;
  CallbackListSubscription subscription;
  {
    RepeatingClosureList cb_reg;
    subscription = cb_reg.Add(
        BindRepeating(&Listener::IncrementTotal, Unretained(&listener)));
    // Make sure the callback is signaled while cb_reg is in scope.
    cb_reg.Notify();
    // Exiting this scope and running the cb_reg destructor shouldn't fail.
  }
  EXPECT_EQ(1, listener.total());

  // Destroying the subscription after the list should not cause any problems.
  subscription = {};
}

// Subscriptions should be movable.
TEST(CallbackListTest, MoveSubscription) {
  RepeatingClosureList cb_reg;
  Listener listener;
  CallbackListSubscription subscription1 = cb_reg.Add(
      BindRepeating(&Listener::IncrementTotal, Unretained(&listener)));
  cb_reg.Notify();
  EXPECT_EQ(1, listener.total());

  auto subscription2 = std::move(subscription1);
  cb_reg.Notify();
  EXPECT_EQ(2, listener.total());

  subscription2 = {};
  cb_reg.Notify();
  EXPECT_EQ(2, listener.total());
}

TEST(CallbackListTest, CancelBeforeRunning) {
  OnceClosureList cb_reg;
  Listener a;

  CallbackListSubscription a_subscription =
      cb_reg.Add(BindOnce(&Listener::IncrementTotal, Unretained(&a)));

  EXPECT_TRUE(a_subscription);

  // Canceling a OnceCallback before running it should not cause problems.
  a_subscription = {};
  cb_reg.Notify();

  // |a| should not have received any callbacks.
  EXPECT_EQ(0, a.total());
}

// Verifies Notify() can be called reentrantly and what its expected effects
// are.
TEST(CallbackListTest, ReentrantNotify) {
  RepeatingClosureList cb_reg;
  Listener a, b, c, d;
  CallbackListSubscription a_subscription, c_subscription;

  // A callback to run for |a|.
  const auto a_callback = [](RepeatingClosureList* callbacks, Listener* a,
                             CallbackListSubscription* a_subscription,
                             const Listener* b, Listener* c,
                             CallbackListSubscription* c_subscription,
                             Listener* d) {
    // This should be the first callback.
    EXPECT_EQ(0, a->total());
    EXPECT_EQ(0, b->total());
    EXPECT_EQ(0, c->total());
    EXPECT_EQ(0, d->total());

    // Increment |a| once.
    a->IncrementTotal();

    // Prevent |a| from being incremented again during the reentrant Notify().
    // Since this is the first callback, this also verifies the inner Notify()
    // doesn't assume the first callback (or all callbacks) are valid.
    *a_subscription = {};

    // Add |c| and |d| to be incremented by the reentrant Notify().
    *c_subscription =
        callbacks->Add(BindRepeating(&Listener::IncrementTotal, Unretained(c)));
    CallbackListSubscription d_subscription =
        callbacks->Add(BindRepeating(&Listener::IncrementTotal, Unretained(d)));

    // Notify reentrantly.  This should not increment |a|, but all the others
    // should be incremented.
    callbacks->Notify();
    EXPECT_EQ(1, b->total());
    EXPECT_EQ(1, c->total());
    EXPECT_EQ(1, d->total());

    // Since |d_subscription| is locally scoped, it should be canceled before
    // the outer Notify() increments |d|.  |c_subscription| already exists and
    // thus |c| should get incremented again by the outer Notify() even though
    // it wasn't scoped when that was called.
  };

  // Add |a| and |b| to the list to be notified, and notify.
  a_subscription = cb_reg.Add(
      BindRepeating(a_callback, Unretained(&cb_reg), Unretained(&a),
                    Unretained(&a_subscription), Unretained(&b), Unretained(&c),
                    Unretained(&c_subscription), Unretained(&d)));
  CallbackListSubscription b_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&b)));

  // Execute both notifications and check the cumulative effect.
  cb_reg.Notify();
  EXPECT_EQ(1, a.total());
  EXPECT_EQ(2, b.total());
  EXPECT_EQ(2, c.total());
  EXPECT_EQ(1, d.total());
}

}  // namespace
}  // namespace base
