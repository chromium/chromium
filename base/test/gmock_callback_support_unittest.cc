// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_callback_support.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByRef;
using testing::MockFunction;

namespace base {
namespace test {

using TestCallback = base::RepeatingCallback<void(const bool& src, bool* dst)>;
using TestOnceCallback = base::OnceCallback<void(const bool& src, bool* dst)>;
using TestOnceCallbackMove =
    base::OnceCallback<void(std::unique_ptr<int>, int* dst)>;

void SetBool(const bool& src, bool* dst) {
  *dst = src;
}

void SetIntFromPtr(std::unique_ptr<int> ptr, int* dst) {
  *dst = *ptr;
}

TEST(GmockCallbackSupportTest, IsNullCallback) {
  MockFunction<void(const TestCallback&)> check;
  EXPECT_CALL(check, Call(IsNullCallback()));
  check.Call(TestCallback());
}

TEST(GmockCallbackSupportTest, IsNotNullCallback) {
  MockFunction<void(const TestCallback&)> check;
  EXPECT_CALL(check, Call(IsNotNullCallback()));
  check.Call(base::BindRepeating(&SetBool));
}

TEST(GmockCallbackSupportTest, IsNullOnceCallback) {
  MockFunction<void(TestOnceCallback)> mock;
  EXPECT_CALL(mock, Call(IsNullCallback()));
  mock.Call(TestOnceCallback());
}

TEST(GmockCallbackSupportTest, IsNotNullOnceCallback) {
  MockFunction<void(TestOnceCallback)> mock;
  EXPECT_CALL(mock, Call(IsNotNullCallback()));
  mock.Call(base::BindOnce(&SetBool));
}

TEST(GmockCallbackSupportTest, RunClosure0) {
  MockFunction<void(const base::RepeatingClosure&)> check;
  bool dst = false;
  EXPECT_CALL(check, Call(IsNotNullCallback())).WillOnce(RunClosure<0>());
  check.Call(base::BindRepeating(&SetBool, true, &dst));
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunClosureByRefNotReset) {
  // Check that RepeatingClosure isn't reset by RunClosure<N>().
  MockFunction<void(base::RepeatingClosure&)> check;
  bool dst = false;
  EXPECT_CALL(check, Call(IsNotNullCallback())).WillOnce(RunClosure<0>());
  auto closure = base::BindRepeating(&SetBool, true, &dst);
  check.Call(closure);
  EXPECT_TRUE(dst);
  EXPECT_FALSE(closure.is_null());
}

TEST(GmockCallbackSupportTest, RunCallback0) {
  MockFunction<void(const TestCallback&)> check;
  bool dst = false;
  EXPECT_CALL(check, Call(IsNotNullCallback()))
      .WillOnce(RunCallback<0>(true, &dst));
  check.Call(base::BindRepeating(&SetBool));
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunCallback1) {
  MockFunction<void(int, const TestCallback&)> check;
  bool dst = false;
  EXPECT_CALL(check, Call(0, IsNotNullCallback()))
      .WillOnce(RunCallback<1>(true, &dst));
  check.Call(0, base::BindRepeating(&SetBool));
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunCallbackPassByRef) {
  MockFunction<void(const TestCallback&)> check;
  bool dst = false;
  bool src = false;
  EXPECT_CALL(check, Call(IsNotNullCallback()))
      .WillOnce(RunCallback<0>(ByRef(src), &dst));
  src = true;
  check.Call(base::BindRepeating(&SetBool));
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunCallbackPassByValue) {
  MockFunction<void(const TestCallback&)> check;
  bool dst = false;
  bool src = true;
  EXPECT_CALL(check, Call(IsNotNullCallback()))
      .WillOnce(RunCallback<0>(src, &dst));
  src = false;
  check.Call(base::BindRepeating(&SetBool));
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunOnceClosure0) {
  MockFunction<void(base::OnceClosure)> check;
  bool dst = false;
  EXPECT_CALL(check, Call(IsNotNullCallback())).WillOnce(RunOnceClosure<0>());
  check.Call(base::BindOnce(&SetBool, true, &dst));
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunOnceCallback0) {
  MockFunction<void(TestOnceCallback)> check;
  bool dst = false;
  bool src = true;
  EXPECT_CALL(check, Call(IsNotNullCallback()))
      .WillOnce(RunOnceCallback<0>(src, &dst));
  src = false;
  check.Call(base::BindOnce(&SetBool));
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunOnceCallbackTwice) {
  MockFunction<void(TestOnceCallback)> check;
  bool dst = false;
  bool src = true;

  EXPECT_CALL(check, Call)
      .WillRepeatedly(RunOnceCallback<0>(std::ref(src), &dst));

  check.Call(base::BindOnce(&SetBool));
  EXPECT_TRUE(dst);

  src = false;
  check.Call(base::BindOnce(&SetBool));
  EXPECT_FALSE(dst);
}

TEST(GmockCallbackSupportTest, RunClosureValue) {
  MockFunction<void()> check;
  bool dst = false;
  EXPECT_CALL(check, Call())
      .WillOnce(RunClosure(base::BindRepeating(&SetBool, true, &dst)));
  check.Call();
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunClosureValueWithArgs) {
  MockFunction<void(bool, int)> check;
  bool dst = false;
  EXPECT_CALL(check, Call(true, 42))
      .WillOnce(RunClosure(base::BindRepeating(&SetBool, true, &dst)));
  check.Call(true, 42);
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunOnceClosureValue) {
  MockFunction<void()> check;
  bool dst = false;
  EXPECT_CALL(check, Call())
      .WillOnce(RunOnceClosure(base::BindOnce(&SetBool, true, &dst)));
  check.Call();
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunOnceClosureValueWithArgs) {
  MockFunction<void(bool, int)> check;
  bool dst = false;
  EXPECT_CALL(check, Call(true, 42))
      .WillOnce(RunOnceClosure(base::BindOnce(&SetBool, true, &dst)));
  check.Call(true, 42);
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunOnceClosureValueMultipleCall) {
  MockFunction<void()> check;
  bool dst = false;
  EXPECT_CALL(check, Call())
      .WillRepeatedly(RunOnceClosure(base::BindOnce(&SetBool, true, &dst)));
  check.Call();
  EXPECT_TRUE(dst);

  // Invoking the RunOnceClosure action more than once will trigger a
  // CHECK-failure.
  dst = false;
  EXPECT_DEATH_IF_SUPPORTED(check.Call(), "");
}

TEST(GmockCallbackSupportTest, RunOnceCallbackWithMoveOnlyType) {
  MockFunction<void(TestOnceCallbackMove)> check;
  auto val = std::make_unique<int>(42);
  int dst = 0;
  EXPECT_CALL(check, Call).WillOnce(RunOnceCallback<0>(std::move(val), &dst));

  check.Call(base::BindOnce(&SetIntFromPtr));

  EXPECT_EQ(dst, 42);
  EXPECT_FALSE(val);
}

TEST(GmockCallbackSupportTest,
     RunOnceCallbackMultipleTimesWithMoveOnlyArgCrashes) {
  MockFunction<void(TestOnceCallbackMove)> check;
  auto val = std::make_unique<int>(42);
  int dst = 0;
  EXPECT_CALL(check, Call)
      .WillRepeatedly(RunOnceCallback<0>(std::move(val), &dst));

  check.Call(base::BindOnce(&SetIntFromPtr));
  EXPECT_EQ(dst, 42);
  EXPECT_FALSE(val);

  // The first `Call` has invalidated the captured std::unique_ptr. Attempting
  // to run `Call` again should result in a runtime crash.
  EXPECT_DEATH_IF_SUPPORTED(check.Call(base::BindOnce(&SetIntFromPtr)), "");
}

TEST(GmockCallbackSupportTest, RunOnceCallbackReturnsValue) {
  MockFunction<int(base::OnceCallback<int(int)>)> check;
  EXPECT_CALL(check, Call).WillRepeatedly(RunOnceCallback<0>(42));
  EXPECT_EQ(43, check.Call(base::BindOnce([](int i) { return i + 1; })));
  EXPECT_EQ(44, check.Call(base::BindOnce([](int i) { return i + 2; })));
  EXPECT_EQ(45, check.Call(base::BindOnce([](int i) { return i + 3; })));
}

TEST(GmockCallbackSupportTest, RunOnceCallbackReturnsValueMoveOnly) {
  MockFunction<int(base::OnceCallback<int(std::unique_ptr<int>)>)> check;
  EXPECT_CALL(check, Call)
      .WillOnce(RunOnceCallback<0>(std::make_unique<int>(42)));
  EXPECT_EQ(43, check.Call(base::BindOnce(
                    [](std::unique_ptr<int> i) { return *i + 1; })));
}

TEST(GmockCallbackSupportTest, RunCallbackReturnsValue) {
  MockFunction<int(base::RepeatingCallback<int(int)>)> check;
  EXPECT_CALL(check, Call).WillRepeatedly(RunCallback<0>(42));
  EXPECT_EQ(43, check.Call(base::BindRepeating([](int i) { return i + 1; })));
  EXPECT_EQ(44, check.Call(base::BindRepeating([](int i) { return i + 2; })));
  EXPECT_EQ(45, check.Call(base::BindRepeating([](int i) { return i + 3; })));
}

}  // namespace test
}  // namespace base
