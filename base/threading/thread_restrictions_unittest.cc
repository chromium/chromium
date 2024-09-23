// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_restrictions.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class ThreadRestrictionsTest : public testing::Test {
 public:
  ThreadRestrictionsTest() = default;

  ThreadRestrictionsTest(const ThreadRestrictionsTest&) = delete;
  ThreadRestrictionsTest& operator=(const ThreadRestrictionsTest&) = delete;

  ~ThreadRestrictionsTest() override {
    internal::ResetThreadRestrictionsForTesting();
  }
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

TEST_F(ThreadRestrictionsTest, BaseSyncPrimitivesAllowedByDefault) {
  internal::AssertBaseSyncPrimitivesAllowed();
}

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

TEST_F(ThreadRestrictionsTest, ScopedDisallowBaseSyncPrimitives) {
  {
    ScopedDisallowBaseSyncPrimitives disallow_sync_primitives;
    EXPECT_DCHECK_DEATH({ internal::AssertBaseSyncPrimitivesAllowed(); });
  }
  internal::AssertBaseSyncPrimitivesAllowed();
}

TEST_F(ThreadRestrictionsTest, SingletonAllowedByDefault) {
  internal::AssertSingletonAllowed();
}

TEST_F(ThreadRestrictionsTest, DisallowSingleton) {
  DisallowSingleton();
  EXPECT_DCHECK_DEATH({ internal::AssertSingletonAllowed(); });
}

TEST_F(ThreadRestrictionsTest, ScopedDisallowSingleton) {
  {
    ScopedDisallowSingleton disallow_sync_primitives;
    EXPECT_DCHECK_DEATH({ internal::AssertSingletonAllowed(); });
  }
  internal::AssertSingletonAllowed();
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

// thread_restriction_checks_and_has_death_tests
#if !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_ANDROID) && DCHECK_IS_ON() && \
    defined(GTEST_HAS_DEATH_TEST)

TEST_F(ThreadRestrictionsTest, BlockingCheckEmitsStack) {
  debug::OverrideStackTraceOutputForTesting enable_stacks_in_death_tests(
      debug::OverrideStackTraceOutputForTesting::Mode::kForceOutput);
  ScopedDisallowBlocking scoped_disallow_blocking;
  // The above ScopedDisallowBlocking should be on the blame list for who set
  // the ban.
  EXPECT_DEATH({ internal::AssertBlockingAllowed(); },
               EXPENSIVE_DCHECKS_ARE_ON() &&
                       debug::StackTrace::WillSymbolizeToStreamForTesting()
                   ? "ScopedDisallowBlocking"
                   : "");
  // And the stack should mention this test body as source.
  EXPECT_DEATH({ internal::AssertBlockingAllowed(); },
               EXPENSIVE_DCHECKS_ARE_ON() &&
                       debug::StackTrace::WillSymbolizeToStreamForTesting()
                   ? "BlockingCheckEmitsStack"
                   : "");
}

class TestCustomDisallow {
 public:
  NOINLINE TestCustomDisallow() { DisallowBlocking(); }
  NOINLINE ~TestCustomDisallow() { PermanentThreadAllowance::AllowBlocking(); }
};

TEST_F(ThreadRestrictionsTest, NestedAllowRestoresPreviousStack) {
  debug::OverrideStackTraceOutputForTesting enable_stacks_in_death_tests(
      debug::OverrideStackTraceOutputForTesting::Mode::kForceOutput);
  TestCustomDisallow custom_disallow;
  {
    ScopedAllowBlocking scoped_allow;
    internal::AssertBlockingAllowed();
  }
  // TestCustomDisallow should be back on the blame list (as opposed to
  // ~ScopedAllowBlocking which is the last one to have changed the state but is
  // no longer relevant).
  EXPECT_DEATH({ internal::AssertBlockingAllowed(); },
               EXPENSIVE_DCHECKS_ARE_ON() &&
                       debug::StackTrace::WillSymbolizeToStreamForTesting()
                   ? "TestCustomDisallow"
                   : "");
  // And the stack should mention this test body as source.
  EXPECT_DEATH({ internal::AssertBlockingAllowed(); },
               EXPENSIVE_DCHECKS_ARE_ON() &&
                       debug::StackTrace::WillSymbolizeToStreamForTesting()
                   ? "NestedAllowRestoresPreviousStack"
                   : "");
}

#endif  // thread_restriction_checks_and_has_death_tests

}  // namespace base
