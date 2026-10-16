---
name: general/avoid-platform-guards-around-dcheck-death-tests
rubric: readability
severity: should_fix
---

**Rule.** In unit tests, use Chromium's portable death test macros from
base/test/gtest_util.h (EXPECT_CHECK_DEATH, EXPECT_DCHECK_DEATH,
EXPECT_NOTREACHED_DEATH, or BASE_EXPECT_DEATH with a specific regex pattern for
general fatal exits) rather than raw EXPECT_DEATH with manual platform
conditionals. Do not add manual platform #if guards (such as #if
BUILDFLAG(IS_IOS) or #if defined(GTEST_HAS_DEATH_TEST) &&
!BUILDFLAG(IS_ANDROID)) or conditional MAYBE\_ / DISABLED\_ macro wrappers
around death tests, as the helper macros handle unsupported platforms and
disabled assertion configurations automatically.

**Why.** Wrapping death tests in manual platform #if guards or
MAYBE\_/DISABLED\_ prefixes is redundant and removes test coverage from
supported platforms. Chromium's death test macros in base/test/gtest_util.h
safely compile test statements and expand to no-op stubs on platforms without
death test support (such as Android or iOS) or when assertions are disabled,
while matching specific failure patterns prevents false passes from unrelated
crashes.

## Examples

Bad:

```cpp
// Bad: Manual platform guard and raw EXPECT_DEATH for fatal exit.
#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)
EXPECT_DEATH({ trigger_fatal_error(); }, ".*");
#endif

// Bad: Conditional MAYBE_ macro around EXPECT_DCHECK_DEATH test.
#if BUILDFLAG(IS_IOS)
#define MAYBE_TimedWait DISABLED_TimedWait
#else
#define MAYBE_TimedWait TimedWait
#endif
TEST_F(WaitableEventTest, MAYBE_TimedWait) {
  EXPECT_DCHECK_DEATH({ event.TimedWait(Milliseconds(10)); });
}
```

Good:

```cpp
// Good: Use BASE_EXPECT_DEATH with a specific pattern.
BASE_EXPECT_DEATH({ trigger_fatal_error(); }, "\\bFATAL\\b.*expected message");

// Good: Unconditional test using EXPECT_DCHECK_DEATH directly.
TEST_F(WaitableEventTest, TimedWait) {
  EXPECT_DCHECK_DEATH({ event.TimedWait(Milliseconds(10)); });
}
```
