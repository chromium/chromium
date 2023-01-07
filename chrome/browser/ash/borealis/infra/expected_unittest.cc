// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/infra/expected.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
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
  Expected<int, bool> exp = Unexpected<int, bool>(true);
  EXPECT_FALSE(exp);
}

TEST(ExpectedTest, UnexpectedWorks) {
  Expected<A, B> a{A()};
  EXPECT_FALSE(a.Unexpected());

  Expected<A, B> b = Unexpected<A, B>(B{});
  EXPECT_TRUE(b.Unexpected());
}

TEST(ExpectedTest, CanHaveSameType) {
  Expected<bool, bool> exp{true};
  EXPECT_FALSE(exp.Unexpected());
  EXPECT_TRUE(exp.Value());

  Expected<bool, bool> unexp = Unexpected<bool, bool>(true);
  EXPECT_TRUE(unexp.Unexpected());
  EXPECT_TRUE(unexp.Error());
}

TEST(ExpectedTest, CanHaveNonCopyableTypes) {
  Expected<std::unique_ptr<int>, std::unique_ptr<bool>> exp{
      std::make_unique<int>(42)};
  EXPECT_FALSE(exp.Unexpected());

  auto unexp = Unexpected<std::unique_ptr<int>, std::unique_ptr<bool>>(
      std::make_unique<bool>(true));
  EXPECT_TRUE(unexp.Unexpected());
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

  Expected<A, B> b = Unexpected<A, B>(B{});
  EXPECT_NE(b.MaybeError(), nullptr);
}

TEST(ExpectedTest, MaybeGettersReturnNullWhenWrong) {
  Expected<A, B> a{A()};
  EXPECT_EQ(a.MaybeError(), nullptr);

  Expected<A, B> b = Unexpected<A, B>(B{});
  EXPECT_EQ(b.MaybeValue(), nullptr);
}

template <typename T>
using CallbackFactory = StrictCallbackFactory<void(T&)>;

TEST(ExpectedTest, HandleCallsCorrectCallback) {
  CallbackFactory<A> a_callback;
  CallbackFactory<B> b_callback;

  EXPECT_CALL(a_callback, Call).Times(1);
  Expected<A, B>{A()}.Handle(
      base::BindOnce(&CallbackFactory<A>::Call, base::Unretained(&a_callback)),
      base::BindOnce(&CallbackFactory<B>::Call, base::Unretained(&b_callback)));

  EXPECT_CALL(b_callback, Call).Times(1);
  Unexpected<A, B>(B{}).Handle(
      base::BindOnce(&CallbackFactory<A>::Call, base::Unretained(&a_callback)),
      base::BindOnce(&CallbackFactory<B>::Call, base::Unretained(&b_callback)));
}

TEST(ExpectedTest, HandleCanReturn) {
  using Exp = Expected<A, B>;
  EXPECT_EQ("expected",
            Exp{A()}.Handle(base::BindOnce([](A&) { return "expected"; }),
                            base::BindOnce([](B&) { return "unexpected"; })));

  EXPECT_EQ("unexpected", Exp::Unexpected(B{}).Handle(
                              base::BindOnce([](A&) { return "expected"; }),
                              base::BindOnce([](B&) { return "unexpected"; })));
}

}  // namespace
}  // namespace borealis
