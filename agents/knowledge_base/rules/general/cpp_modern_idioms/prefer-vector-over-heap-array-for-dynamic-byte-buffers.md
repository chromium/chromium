---
name: general/prefer-vector-over-heap-array-for-dynamic-byte-buffers
rubric: readability
severity: should_fix
---

**Rule.** Prefer standard library containers like std::vector\<uint8_t> over
base::HeapArray\<uint8_t> (or other element types) when storing dynamic byte
buffers, opaque blobs, or general-purpose heap buffers unless fixed-size,
non-resizable unique_ptr\<T[]> replacement semantics without capacity overhead
are specifically required.

**Why.** std::vector is the standard, familiar, and idiomatic C++ container for
owning dynamic contiguous buffers. base::HeapArray is specifically intended as a
bounds-safe replacement for std::unique_ptr\<T[]>; using it for general dynamic
buffers introduces non-standard container semantics without necessity.

## Examples

Bad:

```cpp
// BAD: Using base::HeapArray for general dynamic byte buffer storage.
class SecurityContext {
 private:
  base::HeapArray<uint8_t> permission_info_;
};
```

Good:

```cpp
// GOOD: Use std::vector for general owning dynamic byte storage.
class SecurityContext {
 private:
  std::vector<uint8_t> permission_info_;
};
```
