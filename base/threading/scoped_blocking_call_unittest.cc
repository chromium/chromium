// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/scoped_blocking_call.h"

#include <memory>

#include "base/macros.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class MockBlockingObserver : public internal::BlockingObserver {
 public:
  MockBlockingObserver() = default;

  MOCK_METHOD1(BlockingStarted, void(BlockingType));
  MOCK_METHOD0(BlockingTypeUpgraded, void());
  MOCK_METHOD0(BlockingEnded, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBlockingObserver);
};

class ScopedBlockingCallTest : public testing::Test {
 protected:
  ScopedBlockingCallTest() {
    internal::SetBlockingObserverForCurrentThread(&observer_);
  }

  ~ScopedBlockingCallTest() override {
    internal::ClearBlockingObserverForCurrentThread();
  }

  testing::StrictMock<MockBlockingObserver> observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedBlockingCallTest);
};

}  // namespace

TEST_F(ScopedBlockingCallTest, MayBlock) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::MAY_BLOCK));
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_CALL(observer_, BlockingEnded());
}

TEST_F(ScopedBlockingCallTest, WillBlock) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::WILL_BLOCK));
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::WILL_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_CALL(observer_, BlockingEnded());
}

TEST_F(ScopedBlockingCallTest, MayBlockWillBlock) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::MAY_BLOCK));
  ScopedBlockingCall scoped_blocking_call_a(FROM_HERE, BlockingType::MAY_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);

  {
    EXPECT_CALL(observer_, BlockingTypeUpgraded());
    ScopedBlockingCall scoped_blocking_call_b(FROM_HERE,
                                              BlockingType::WILL_BLOCK);
    testing::Mock::VerifyAndClear(&observer_);
  }

  EXPECT_CALL(observer_, BlockingEnded());
}

TEST_F(ScopedBlockingCallTest, WillBlockMayBlock) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::WILL_BLOCK));
  ScopedBlockingCall scoped_blocking_call_a(FROM_HERE,
                                            BlockingType::WILL_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);

  {
    ScopedBlockingCall scoped_blocking_call_b(FROM_HERE,
                                              BlockingType::MAY_BLOCK);
  }

  EXPECT_CALL(observer_, BlockingEnded());
}

TEST_F(ScopedBlockingCallTest, MayBlockMayBlock) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::MAY_BLOCK));
  ScopedBlockingCall scoped_blocking_call_a(FROM_HERE, BlockingType::MAY_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);

  {
    ScopedBlockingCall scoped_blocking_call_b(FROM_HERE,
                                              BlockingType::MAY_BLOCK);
  }

  EXPECT_CALL(observer_, BlockingEnded());
}

TEST_F(ScopedBlockingCallTest, WillBlockWillBlock) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::WILL_BLOCK));
  ScopedBlockingCall scoped_blocking_call_a(FROM_HERE,
                                            BlockingType::WILL_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);

  {
    ScopedBlockingCall scoped_blocking_call_b(FROM_HERE,
                                              BlockingType::WILL_BLOCK);
  }

  EXPECT_CALL(observer_, BlockingEnded());
}

TEST_F(ScopedBlockingCallTest, MayBlockWillBlockTwice) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::MAY_BLOCK));
  ScopedBlockingCall scoped_blocking_call_a(FROM_HERE, BlockingType::MAY_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);

  {
    EXPECT_CALL(observer_, BlockingTypeUpgraded());
    ScopedBlockingCall scoped_blocking_call_b(FROM_HERE,
                                              BlockingType::WILL_BLOCK);
    testing::Mock::VerifyAndClear(&observer_);

    {
      ScopedBlockingCall scoped_blocking_call_c(FROM_HERE,
                                                BlockingType::MAY_BLOCK);
      ScopedBlockingCall scoped_blocking_call_d(FROM_HERE,
                                                BlockingType::WILL_BLOCK);
    }
  }

  EXPECT_CALL(observer_, BlockingEnded());
}

TEST(ScopedBlockingCallDestructionOrderTest, InvalidDestructionOrder) {
  auto scoped_blocking_call_a =
      std::make_unique<ScopedBlockingCall>(FROM_HERE, BlockingType::WILL_BLOCK);
  auto scoped_blocking_call_b =
      std::make_unique<ScopedBlockingCall>(FROM_HERE, BlockingType::WILL_BLOCK);

  EXPECT_DCHECK_DEATH({ scoped_blocking_call_a.reset(); });
}

}  // namespace base
