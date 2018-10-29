// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_restrictions.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class ThreadRestrictionsTest : public testing::Test {
 public:
  ThreadRestrictionsTest() = default;
  ~ThreadRestrictionsTest() override {
    internal::ResetThreadRestrictionsForTesting();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadRestrictionsTest);
};

}  // namespace

TEST_F(ThreadRestrictionsTest, BlockingAllowedByDefault) {
  internal::AssertBlockingAllowed();
}

TEST_F(ThreadRestrictionsTest, ScopedDisallowBlocking) {
  {
    ScopedDisallowBlocking scoped_disallow_blocking;
    EXPECT_DCHECK_DEATH({ internal::AssertBlockingAllowed(); });
  }
  internal::AssertBlockingAllowed();
}

TEST_F(ThreadRestrictionsTest, ScopedAllowBlocking) {
  ScopedDisallowBlocking scoped_disallow_blocking;
  {
    ScopedAllowBlocking scoped_allow_blocking;
    internal::AssertBlockingAllowed();
  }
  EXPECT_DCHECK_DEATH({ internal::AssertBlockingAllowed(); });
}

TEST_F(ThreadRestrictionsTest, ScopedAllowBlockingForTesting) {
  ScopedDisallowBlocking scoped_disallow_blocking;
  {
    ScopedAllowBlockingForTesting scoped_allow_blocking_for_testing;
    internal::AssertBlockingAllowed();
  }
  EXPECT_DCHECK_DEATH({ internal::AssertBlockingAllowed(); });
}

TEST_F(ThreadRestrictionsTest, BaseSyncPrimitivesAllowedByDefault) {}

TEST_F(ThreadRestrictionsTest, DisallowBaseSyncPrimitives) {
  DisallowBaseSyncPrimitives();
  EXPECT_DCHECK_DEATH({ internal::AssertBaseSyncPrimitivesAllowed(); });
}

TEST_F(ThreadRestrictionsTest, ScopedAllowBaseSyncPrimitives) {
  DisallowBaseSyncPrimitives();
  ScopedAllowBaseSyncPrimitives scoped_allow_base_sync_primitives;
  internal::AssertBaseSyncPrimitivesAllowed();
}

TEST_F(ThreadRestrictionsTest, ScopedAllowBaseSyncPrimitivesResetsState) {
  DisallowBaseSyncPrimitives();
  { ScopedAllowBaseSyncPrimitives scoped_allow_base_sync_primitives; }
  EXPECT_DCHECK_DEATH({ internal::AssertBaseSyncPrimitivesAllowed(); });
}

TEST_F(ThreadRestrictionsTest,
       ScopedAllowBaseSyncPrimitivesWithBlockingDisallowed) {
  ScopedDisallowBlocking scoped_disallow_blocking;
  DisallowBaseSyncPrimitives();

  // This should DCHECK because blocking is not allowed in this scope
  // and OutsideBlockingScope is not passed to the constructor.
  EXPECT_DCHECK_DEATH(
      { ScopedAllowBaseSyncPrimitives scoped_allow_base_sync_primitives; });
}

TEST_F(ThreadRestrictionsTest,
       ScopedAllowBaseSyncPrimitivesOutsideBlockingScope) {
  ScopedDisallowBlocking scoped_disallow_blocking;
  DisallowBaseSyncPrimitives();
  ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
      scoped_allow_base_sync_primitives;
  internal::AssertBaseSyncPrimitivesAllowed();
}

TEST_F(ThreadRestrictionsTest,
       ScopedAllowBaseSyncPrimitivesOutsideBlockingScopeResetsState) {
  DisallowBaseSyncPrimitives();
  {
    ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
        scoped_allow_base_sync_primitives;
  }
  EXPECT_DCHECK_DEATH({ internal::AssertBaseSyncPrimitivesAllowed(); });
}

TEST_F(ThreadRestrictionsTest, ScopedAllowBaseSyncPrimitivesForTesting) {
  DisallowBaseSyncPrimitives();
  ScopedAllowBaseSyncPrimitivesForTesting
      scoped_allow_base_sync_primitives_for_testing;
  internal::AssertBaseSyncPrimitivesAllowed();
}

TEST_F(ThreadRestrictionsTest,
       ScopedAllowBaseSyncPrimitivesForTestingResetsState) {
  DisallowBaseSyncPrimitives();
  {
    ScopedAllowBaseSyncPrimitivesForTesting
        scoped_allow_base_sync_primitives_for_testing;
  }
  EXPECT_DCHECK_DEATH({ internal::AssertBaseSyncPrimitivesAllowed(); });
}

TEST_F(ThreadRestrictionsTest,
       ScopedAllowBaseSyncPrimitivesForTestingWithBlockingDisallowed) {
  ScopedDisallowBlocking scoped_disallow_blocking;
  DisallowBaseSyncPrimitives();
  // This should not DCHECK.
  ScopedAllowBaseSyncPrimitivesForTesting
      scoped_allow_base_sync_primitives_for_testing;
}

TEST_F(ThreadRestrictionsTest, LongCPUWorkAllowedByDefault) {
  AssertLongCPUWorkAllowed();
}

TEST_F(ThreadRestrictionsTest, DisallowUnresponsiveTasks) {
  DisallowUnresponsiveTasks();
  EXPECT_DCHECK_DEATH(internal::AssertBlockingAllowed());
  EXPECT_DCHECK_DEATH(internal::AssertBaseSyncPrimitivesAllowed());
  EXPECT_DCHECK_DEATH(AssertLongCPUWorkAllowed());
}

}  // namespace base
