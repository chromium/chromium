// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_move_support.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::DoAll;
using ::testing::Pointee;

using MoveOnly = std::unique_ptr<int>;

struct MockFoo {
  MOCK_METHOD(void, ByRef, (MoveOnly&), ());
  MOCK_METHOD(void, ByVal, (MoveOnly), ());
  MOCK_METHOD(void, TwiceByRef, (MoveOnly&, MoveOnly&), ());
  MOCK_METHOD(bool, ByValWithReturnValue, (MoveOnly), ());
};
}  // namespace

TEST(GmockMoveSupportTest, MoveArgByRef) {
  MoveOnly result;

  MockFoo foo;
  EXPECT_CALL(foo, ByRef).WillOnce(MoveArg(&result));
  MoveOnly arg = std::make_unique<int>(123);
  foo.ByRef(arg);

  EXPECT_THAT(result, Pointee(123));
}

TEST(GmockMoveSupportTest, MoveArgByVal) {
  MoveOnly result;

  MockFoo foo;
  EXPECT_CALL(foo, ByVal).WillOnce(MoveArg(&result));
  foo.ByVal(std::make_unique<int>(456));

  EXPECT_THAT(result, Pointee(456));
}

TEST(GmockMoveSupportTest, MoveArgsTwiceByRef) {
  MoveOnly result1;
  MoveOnly result2;

  MockFoo foo;
  EXPECT_CALL(foo, TwiceByRef)
      .WillOnce(DoAll(MoveArg<0>(&result1), MoveArg<1>(&result2)));
  MoveOnly arg1 = std::make_unique<int>(123);
  MoveOnly arg2 = std::make_unique<int>(456);
  foo.TwiceByRef(arg1, arg2);

  EXPECT_THAT(result1, Pointee(123));
  EXPECT_THAT(result2, Pointee(456));
}

TEST(GmockMoveSupportTest, MoveArgAndReturn) {
  MoveOnly result;

  MockFoo foo;
  EXPECT_CALL(foo, ByValWithReturnValue)
      .WillOnce(MoveArgAndReturn(&result, true));
  EXPECT_TRUE(foo.ByValWithReturnValue(std::make_unique<int>(123)));

  EXPECT_THAT(result, Pointee(123));
}
