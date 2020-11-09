// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/infra/expected.h"

#include "base/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {
namespace {

struct A {};

struct B {};

TEST(ExpectedTest, CanConstructWithExpected) {
  Expected<int, bool> exp(42);
  EXPECT_TRUE(exp);
}

TEST(ExpectedTest, CanConstructWithUnexpected) {
  Expected<int, bool> exp = Unexpected<int>(true);
  EXPECT_FALSE(exp);
}

TEST(ExpectedTest, UnexpectedWorks) {
  Expected<A, B> a{A()};
  EXPECT_FALSE(a.Unexpected());

  Expected<A, B> b = Unexpected<A>(B{});
  EXPECT_TRUE(b.Unexpected());
}

TEST(ExpectedTest, GettersReturnCorrectValues) {
  Expected<std::string, void*> val{"rumpelstiltskin"};
  EXPECT_EQ(val.Value(), "rumpelstiltskin");

  Expected<std::string, void*> err = Unexpected<std::string, void*>(nullptr);
  EXPECT_EQ(err.Error(), nullptr);
}

TEST(ExpectedTest, MaybeGettersReturnPointerWhenRight) {
  Expected<A, B> a{A()};
  EXPECT_NE(a.MaybeValue(), nullptr);

  Expected<A, B> b = Unexpected<A>(B{});
  EXPECT_NE(b.MaybeError(), nullptr);
}

TEST(ExpectedTest, MaybeGettersReturnNullWhenWrong) {
  Expected<A, B> a{A()};
  EXPECT_EQ(a.MaybeError(), nullptr);

  Expected<A, B> b = Unexpected<A>(B{});
  EXPECT_EQ(b.MaybeValue(), nullptr);
}

template <typename T>
using CallbackFactory = testing::StrictMock<testing::MockFunction<void(T&)>>;

TEST(ExpectedTest, HandleCallsCorrectCallback) {
  CallbackFactory<A> a_callback;
  CallbackFactory<B> b_callback;

  Expected<A, B> a{A()};
  EXPECT_CALL(a_callback, Call).Times(1);
  a.Handle(
      base::BindOnce(&CallbackFactory<A>::Call, base::Unretained(&a_callback)),
      base::BindOnce(&CallbackFactory<B>::Call, base::Unretained(&b_callback)));

  Expected<A, B> b = Unexpected<A>(B{});
  EXPECT_CALL(b_callback, Call).Times(1);
  b.Handle(
      base::BindOnce(&CallbackFactory<A>::Call, base::Unretained(&a_callback)),
      base::BindOnce(&CallbackFactory<B>::Call, base::Unretained(&b_callback)));
}

}  // namespace
}  // namespace borealis
