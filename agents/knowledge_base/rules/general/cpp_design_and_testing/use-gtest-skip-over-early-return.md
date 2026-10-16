---
name: general/use-gtest-skip-over-early-return
rubric: readability
severity: blocking
---

**Rule.** When a GoogleTest cannot run due to unsupported CPU architectures,
OS/kernel versions, missing hardware features, or attached debuggers, invoke
`GTEST_SKIP()` with an explanatory message rather than silently returning early,
weakening production assertions, or bundling the condition into
`DISABLED_`/`MAYBE_` macros tracked by bug `TODO`s. Additionally, when adding
environment- or platform-specific skips, audit the test suite to apply the skip
consistently across all counterpart tests sharing the same dependency.

**Why.** Silently returning records a false-positive pass without diagnostic
logs, weakening assertions hides production bugs, and bundling unsupported
platforms into `DISABLED_` macros conflates permanent architectural limitations
with temporary bug tracking. Calling `GTEST_SKIP()` clearly records the skip
reason in test logs while preserving strict production invariants.

## Examples

Bad:

```cpp
// Bad: Bundling unsupported arch into bug-tracked MAYBE_ macro instead of GTEST_SKIP():
// TODO(crbug.com/123456): Flaky on Mac.
#if BUILDFLAG(IS_APPLE) || (!defined(ARCH_CPU_X86_FAMILY) && !defined(ARCH_CPU_ARM_FAMILY))
#define MAYBE_SpinLock DISABLED_SpinLock
#endif

// Bad: Weakening production assertions or returning early instead of skipping:
PA_DCHECK(futex_errno != EINVAL);  // Removed != ENOSYS
if (base::debug::BeingDebugged()) {
  return;  // Records a false pass without logs
}
```

Good:

```cpp
// Good: Keep bug-tracked MAYBE_ macros separate from unsupported architecture skips:
// TODO(crbug.com/123456): Flaky on Mac.
#if BUILDFLAG(IS_APPLE)
#define MAYBE_SpinLock DISABLED_SpinLock
#endif
TEST_F(LockTest, MAYBE_SpinLock) {
#if !defined(ARCH_CPU_X86_FAMILY) && !defined(ARCH_CPU_ARM_FAMILY)
  GTEST_SKIP() << "Spinning unsupported on this CPU architecture.";
#endif
  ...
}

// Good: Retain strict DCHECKs, invoke GTEST_SKIP(), and audit counterpart tests:
PA_DCHECK(futex_errno != EINVAL && futex_errno != ENOSYS);
if (base::debug::BeingDebugged()) {
  GTEST_SKIP() << "Skipping test when attached to debugger.";
}
```
