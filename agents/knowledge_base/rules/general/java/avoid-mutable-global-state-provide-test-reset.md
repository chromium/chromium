---
name: general/avoid-mutable-global-state-provide-test-reset
rubric: correctness
severity: should_fix
---

**Rule.** Do not introduce un-resettable mutable process-global variables;
prefer dependency injection, scope thread-affine state to `thread_local` (or
`base::SequenceLocalStorageSlot`), provide dedicated `Reset...ForTesting()`
helpers, or register state restoration in Java via
`ResettersForTesting.register(...)`. Expose reusable setup/teardown helpers and
RAII runners (such as
`[[nodiscard]] base::ScopedClosureRunner Init...ForTesting()`) in public
component headers rather than private test helpers. In test methods, fixtures,
and runner lifecycle hooks, execute state teardown and cleanup unconditionally
via fixture hooks (`@After` `tearDown()`, `TearDown()`, `ClassCleanupHook`) or
RAII guards (`base::AutoReset`, `base::ScopedClosureRunner`) rather than manual
statements at the end of test bodies or gating cleanup behind test success
conditionals; gate only post-test verification checks (such as `LifetimeAssert`
or `AfterCleanupCheck`) on test success.

**Why.** Mutable process-global state persists across tests, causing test
pollution and order-dependent flakiness. Trailing cleanup statements in test
method bodies or runner teardown hooks gated behind test-success conditionals
are bypassed on test failures, leaking dirty state to subsequent tests;
executing cleanup unconditionally guarantees reliable state restoration while
gating only verification assertions avoids spurious secondary failures.

## Examples

Bad:

```cpp
// C++: Manual cleanup at the end of a test is bypassed on ASSERT_* failure.
TEST_F(MyTest, ModifiesGlobalState) {
  InitGlobalState();
  ASSERT_TRUE(DoOperation());
  ClearGlobalStateForTesting();
}

// Java: Teardown hook gated behind test success leaves dirty state on failure.
if (afterClassPassed && !mAnyTestFailed) {
  for (ClassCleanupHook hook : getClassCleanupHooks()) {
    hook.onAfterTestClass(getTestClass().getJavaClass());
  }
}
```

Good:

```cpp
// C++: Manage cleanup via RAII or fixture TearDown() override.
TEST_F(MyTest, ModifiesGlobalState) {
  base::ScopedClosureRunner reset = variations::InitCrashKeysForTesting();
  ASSERT_TRUE(DoOperation());
  EXPECT_EQ(GetGlobalState(), 1);
}

// Java: Execute cleanup unconditionally; gate only verification checks.
for (ClassCleanupHook hook : getClassCleanupHooks()) {
  hook.onAfterTestClass(getTestClass().getJavaClass());
}
if (afterClassPassed && !mAnyTestFailed) {
  LifetimeAssert.assertAllInstancesDestroyedForTesting();
}
```
