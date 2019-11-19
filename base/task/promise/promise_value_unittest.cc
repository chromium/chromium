// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/promise/promise_value.h"

#include "base/task/promise/no_op_promise_executor.h"
#include "base/test/copy_only_int.h"
#include "base/test/do_nothing_promise.h"
#include "base/test/move_only_int.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

struct IntMoveOnlyCopyOnlyInt {
  IntMoveOnlyCopyOnlyInt(int value,
                         MoveOnlyInt /*move_only*/,
                         CopyOnlyInt /*copy_only*/)
      : value(value) {}

  int value;
};

class DestructDetector {
 public:
  explicit DestructDetector(bool* destructor_called)
      : destructor_called_(destructor_called) {}

  ~DestructDetector() { *destructor_called_ = true; }

 private:
  bool* destructor_called_;  // NOT OWNED
};

}  // namespace

TEST(PromiseValueTest, Noexcept) {
  static_assert(std::is_nothrow_default_constructible<PromiseValue>(), "");
  static_assert(std::is_nothrow_move_constructible<PromiseValue>(), "");
  static_assert(std::is_nothrow_move_assignable<PromiseValue>(), "");
  static_assert(noexcept(std::declval<PromiseValue&>().has_value()), "");
  static_assert(noexcept(std::declval<PromiseValue&>().Get<Resolved<int>>()),
                "");
}

TEST(PromiseValueTest, HasValue) {
  PromiseValue o;
  EXPECT_FALSE(o.has_value());
  o = Resolved<int>();
  EXPECT_TRUE(o.has_value());
  o.reset();
  EXPECT_FALSE(o.has_value());
}

TEST(PromiseValueTest, Construction) {
  PromiseValue value(in_place_type_t<PromiseExecutor>(),
                     PromiseExecutor::Data(
                         in_place_type_t<NoOpPromiseExecutor>(), true, true));

  EXPECT_TRUE(value.has_value());
  EXPECT_TRUE(value.ContainsPromiseExecutor());
  EXPECT_FALSE(value.ContainsCurriedPromise());
  EXPECT_FALSE(value.ContainsResolved());
  EXPECT_FALSE(value.ContainsRejected());
}

TEST(PromiseValueTest, InPlaceConstruction) {
  const CopyOnlyInt copy_only{};
  PromiseValue o(in_place_type_t<Resolved<IntMoveOnlyCopyOnlyInt>>(), 5,
                 MoveOnlyInt(), copy_only);
  IntMoveOnlyCopyOnlyInt& v = o.Get<Resolved<IntMoveOnlyCopyOnlyInt>>()->value;
  EXPECT_EQ(5, v.value);
}

TEST(PromiseValueTest, EmplaceInplaceType) {
  PromiseValue value;
  EXPECT_FALSE(value.has_value());

  value.emplace(in_place_type_t<Rejected<int>>(), 123);
  EXPECT_TRUE(value.has_value());
  EXPECT_FALSE(value.ContainsPromiseExecutor());
  EXPECT_FALSE(value.ContainsCurriedPromise());
  EXPECT_FALSE(value.ContainsResolved());
  EXPECT_TRUE(value.ContainsRejected());
  EXPECT_EQ(123, value.template Get<Rejected<int>>()->value);
}

TEST(PromiseValueTest, Reset) {
  PromiseValue value(in_place_type_t<PromiseExecutor>(),
                     PromiseExecutor::Data(
                         in_place_type_t<NoOpPromiseExecutor>(), true, true));

  EXPECT_TRUE(value.has_value());
  value.reset();

  EXPECT_FALSE(value.has_value());
  EXPECT_FALSE(value.ContainsPromiseExecutor());
  EXPECT_FALSE(value.ContainsCurriedPromise());
  EXPECT_FALSE(value.ContainsResolved());
  EXPECT_FALSE(value.ContainsRejected());
}

TEST(PromiseValueTest, AssignCurriedPromise) {
  PromiseValue value(in_place_type_t<PromiseExecutor>(),
                     PromiseExecutor::Data(
                         in_place_type_t<NoOpPromiseExecutor>(), true, true));
  scoped_refptr<AbstractPromise> promise = DoNothingPromiseBuilder(FROM_HERE);

  EXPECT_FALSE(value.ContainsCurriedPromise());
  value = promise;

  EXPECT_TRUE(value.ContainsCurriedPromise());
  EXPECT_EQ(promise, *value.Get<scoped_refptr<AbstractPromise>>());
}

TEST(PromiseValueTest, AssignResolvedValue) {
  PromiseValue value(in_place_type_t<PromiseExecutor>(),
                     PromiseExecutor::Data(
                         in_place_type_t<NoOpPromiseExecutor>(), true, true));

  EXPECT_FALSE(value.ContainsResolved());
  value = base::Resolved<int>(123);

  EXPECT_TRUE(value.ContainsResolved());
  EXPECT_EQ(123, value.Get<Resolved<int>>()->value);
}

TEST(PromiseValueTest, AssignRejectedValue) {
  PromiseValue value(in_place_type_t<PromiseExecutor>(),
                     PromiseExecutor::Data(
                         in_place_type_t<NoOpPromiseExecutor>(), true, true));

  EXPECT_FALSE(value.ContainsRejected());
  value = base::Rejected<int>(123);

  EXPECT_TRUE(value.ContainsRejected());
  EXPECT_EQ(123, value.Get<Rejected<int>>()->value);
}

TEST(PromiseValueTest, EmplaceRejectedTuple) {
  PromiseValue value(in_place_type_t<PromiseExecutor>(),
                     PromiseExecutor::Data(
                         in_place_type_t<NoOpPromiseExecutor>(), true, true));

  EXPECT_FALSE(value.ContainsRejected());
  value.emplace(in_place_type_t<base::Rejected<std::tuple<int, std::string>>>(),
                123, "Hello");

  EXPECT_TRUE(value.ContainsRejected());
  EXPECT_EQ(
      123,
      std::get<0>(value.Get<Rejected<std::tuple<int, std::string>>>()->value));
  EXPECT_EQ(
      "Hello",
      std::get<1>(value.Get<Rejected<std::tuple<int, std::string>>>()->value));
}

TEST(PromiseValueTest, ConversionConstruction) {
  {
    PromiseValue o(Rejected<int>(3));
    EXPECT_EQ(3, o.Get<Rejected<int>>()->value);
  }

  {
    const CopyOnlyInt copy_only(5);
    PromiseValue o = PromiseValue(Resolved<CopyOnlyInt>(copy_only));
    EXPECT_EQ(5, o.Get<Resolved<CopyOnlyInt>>()->value.data());
  }

  {
    MoveOnlyInt i{123};
    PromiseValue o(Rejected<MoveOnlyInt>(std::move(i)));
    EXPECT_EQ(123, o.Get<Rejected<MoveOnlyInt>>()->value.data());
  }
}

TEST(PromiseValueTest, ConversionAssignment) {
  {
    PromiseValue o;
    o = Rejected<int>(3);
    EXPECT_EQ(3, o.Get<Rejected<int>>()->value);
  }

  {
    const CopyOnlyInt copy_only(5);
    PromiseValue o;
    o = Resolved<CopyOnlyInt>(copy_only);
    EXPECT_EQ(5, o.Get<Resolved<CopyOnlyInt>>()->value.data());
  }

  {
    MoveOnlyInt i{123};
    PromiseValue o;
    o = Rejected<MoveOnlyInt>(std::move(i));
    EXPECT_EQ(123, o.Get<Rejected<MoveOnlyInt>>()->value.data());
  }
}

TEST(PromiseValueTest, DestructorCalled) {
  bool destructor_called = false;

  {
    PromiseValue a;
    a.emplace(in_place_type_t<Resolved<DestructDetector>>(),
              &destructor_called);
    EXPECT_FALSE(destructor_called);
  }

  EXPECT_TRUE(destructor_called);
}

TEST(PromiseValueTest, DestructorCalledOnAssignment) {
  bool destructor_called = false;

  PromiseValue a;
  a.emplace(in_place_type_t<Rejected<DestructDetector>>(), &destructor_called);

  EXPECT_FALSE(destructor_called);
  a = Resolved<int>(123);
  EXPECT_TRUE(destructor_called);
}

}  // namespace internal
}  // namespace base
