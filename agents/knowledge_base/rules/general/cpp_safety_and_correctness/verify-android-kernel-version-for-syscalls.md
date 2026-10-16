---
name: general/verify-android-kernel-version-for-syscalls
rubric: correctness
severity: should_fix
---

**Rule.** When determining whether a Linux kernel feature or syscall (such as
`getrandom(2)`, `futex`, or `pidfd`) is available on Android, do not assume the
Linux kernel version based solely on the minimum supported Android API level
(`minSdkVersion` or `android_get_device_api_level()`). Check the runtime kernel
version (e.g., via `base::SysInfo::KernelVersionNumber::Current()`) or handle
`ENOSYS` fallback paths, and account for older kernel versions supported by
embedders such as Cronet.

**Why.** OEM devices upgraded to newer Android API levels frequently retain
their original older Linux kernels (e.g., kernel 3.10 on Android M/N).
Furthermore, standalone embedders like Cronet maintain support for older Android
releases longer than Chromium, so assuming kernel syscall availability without
runtime kernel version checks or fallback mechanisms risks crashes or broken
functionality on older kernels.

## Examples

Bad:

```cpp
// Bad: Assumes minimum supported Android API level guarantees Linux kernel >= 3.17.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
if (syscall(__NR_getrandom, output, length, 0) == -1) {
  DPCHECK(errno != ENOSYS);
}
#endif
```

Good:

```cpp
// Good: Checks runtime kernel version before invoking getrandom(2)
// and provides fallback to /dev/urandom for older Android kernels.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
static const bool kHasSupport =
    base::SysInfo::KernelVersionNumber::Current() >=
    base::SysInfo::KernelVersionNumber(3, 17);
if (kHasSupport && HANDLE_EINTR(syscall(__NR_getrandom, output, length, 0)) == length) {
  return true;
}
return ReadFromFD(GetUrandomFD(), span);
#endif
```
