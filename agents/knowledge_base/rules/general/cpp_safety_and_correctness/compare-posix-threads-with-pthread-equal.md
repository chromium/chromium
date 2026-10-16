---
name: general/compare-posix-threads-with-pthread-equal
rubric: correctness
severity: should_fix
---

**Rule.** Do not compare POSIX thread handles (pthread_t) directly using
operator== or operator!=. Use pthread_equal() to test pthread_t values for
equality portably.

**Why.** POSIX specifies pthread_t as an opaque type that is not guaranteed to
be an arithmetic type. Direct comparison with operator== is non-portable,
violates the POSIX specification, and can fail to compile on platforms where
pthread_t is represented as a structure.

## Examples

Bad:

```cpp
bool is_equal(const PlatformThreadHandle& other) const {
  return handle_ == other.handle_;
}
```

Good:

```cpp
bool is_equal(const PlatformThreadHandle& other) const {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return pthread_equal(handle_, other.handle_);
#else
  return handle_ == other.handle_;
#endif
}
```
