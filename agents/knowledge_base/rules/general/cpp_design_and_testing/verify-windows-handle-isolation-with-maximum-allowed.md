---
name: general/verify-windows-handle-isolation-with-maximum-allowed
rubric: testing
severity: optional
---

**Rule.** When testing Windows process, thread, or handle isolation, open the
target handle using MAXIMUM_ALLOWED and inspect
base::win::GetGrantedAccess(handle) to verify expected safe rights and ensure
dangerous rights are denied. Do not merely assert that opening a handle with
standard or default access fails.

**Why.** Asserting that opening an object with a default access mask fails only
verifies that requesting that specific combination was rejected, without
verifying what rights the security descriptor actually permits. Opening with
MAXIMUM_ALLOWED retrieves all permitted access rights, enabling tests to verify
both the presence of safe rights and the absence of dangerous rights.

## Examples

Bad:

```cpp
const auto writeable_process = base::Process::Open(pid);
EXPECT_EQ(kExpectFullIsolation, !writeable_process.IsValid());
```

Good:

```cpp
const auto maximum_allowed_process =
    base::Process::OpenWithAccess(pid, MAXIMUM_ALLOWED);
ASSERT_TRUE(maximum_allowed_process.IsValid());
const auto access =
    base::win::GetGrantedAccess(maximum_allowed_process.Handle());
ASSERT_TRUE(access.has_value());
for (const auto safe_mask : safe_masks) {
  EXPECT_TRUE(*access & safe_mask);
}
for (const auto unsafe_mask : unsafe_masks) {
  EXPECT_EQ(kExpectFullIsolation, !(*access & unsafe_mask));
}
```
