---
name: general/remove-platform-guards-when-disabling-gtests-unconditionally
rubric: correctness
severity: blocking
---

**Rule.** When disabling a GoogleTest test case unconditionally across all
platforms by prefixing its name with DISABLED\_, remove surrounding platform #if
/ #endif preprocessor guards completely. Do not leave orphaned or conditional
platform preprocessor blocks around test cases that are already prefixed with
DISABLED\_.

**Why.** Prefixing a test directly with DISABLED\_ disables it unconditionally
across all platforms while keeping it compiled and tracked in test results.
Leaving orphaned or conditional #if guards around DISABLED\_ tests breaks
compilation or prevents other platforms from compiling and reporting the
disabled test.

## Examples

Bad:

```cpp
// TODO(crbug.com/123456): Failed on ChromeOS and Tsan builders.
#if BUILDFLAG(IS_CHROMEOS)
TEST_F(StackSamplerTest, DISABLED_AuxUnwinderInvokedWhileRecordingStackFrames) {
  // ...
}
```

Good:

```cpp
// TODO(crbug.com/123456): Failed on ChromeOS and Tsan builders.
TEST_F(StackSamplerTest, DISABLED_AuxUnwinderInvokedWhileRecordingStackFrames) {
  // ...
}
```
