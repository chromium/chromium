---
name: base/prefer-getentropy-over-dev-urandom-on-macos
rubric: safety
severity: should_fix
---

**Rule.** When collecting random bytes or implementing allocation-free entropy
sources on macOS, call getentropy() directly rather than reading from
/dev/urandom or maintaining a dedicated /dev/urandom file descriptor. Because
getentropy(2) is available across all supported macOS versions, invoke it
directly without runtime OS version checks or fallback paths.

**Why.** Opening or maintaining open file descriptors for /dev/urandom consumes
process file descriptor table slots and incurs filesystem I/O overhead. Calling
getentropy(2) directly provides cryptographically secure entropy with minimal
syscall overhead, avoids leaking file descriptors across fork or process
boundaries, and simplifies platform-specific entropy code.

## Examples

Bad:

```cpp
// Reading from /dev/urandom consumes a file descriptor and incurs I/O overhead.
CHECK(ReadFromFD(GetUrandomFD(), as_writable_chars(span_from_ref(number))));
return RandomUint64ToRandomDouble(number);
```

Good:

```cpp
#if BUILDFLAG(IS_MAC) || PA_BUILDFLAG(IS_MAC)
if (getentropy(&number, sizeof(number)) == 0) {
  return RandomUint64ToRandomDouble(number);
}
#endif
CHECK(ReadFromFD(GetUrandomFD(), as_writable_chars(span_from_ref(number))));
return RandomUint64ToRandomDouble(number);
```
