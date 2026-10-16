---
name: general/expose-span-accessors-for-buffer-classes
rubric: safety
severity: should_fix
---

**Rule.** Expose underlying storage of buffer- or memory-managing classes
through span-returning accessors (such as `.bytes()`, `.span()`, or
`.as_byte_span()`) rather than requiring callers to construct `base::span` from
raw `.data()` pointers and sizes using `UNSAFE_BUFFERS` or `UNSAFE_TODO`. In
callers, use these span accessors and subspan methods instead of wrapping raw
pointer conversions in unsafe buffer macros.

**Why.** Constructing `base::span` from raw pointer and size pairs at call sites
requires `UNSAFE_BUFFERS` or `UNSAFE_TODO` macro suppressions, spreading unsafe
annotations throughout the codebase. Encapsulating span creation inside the
owning class isolates unsafe pointer conversions to a single audited method and
provides callers with a safe, bounds-checked range API.

## Examples

Bad:

```cpp
// Caller forced to construct span from raw pointer and length:
ASSERT_TRUE(writer
                .Write(0, UNSAFE_TODO(base::span(
                              static_cast<const uint8_t*>(allocator.data()),
                              filesize)))
                .has_value());
```

Good:

```cpp
// Buffer class exposes safe span accessor:
// class PersistentMemoryAllocator {
//  public:
//   base::span<const uint8_t> bytes() const;
// };

ASSERT_TRUE(writer.Write(0, allocator.bytes().first(filesize)).has_value());
```
