---
name: general/validate-pointer-and-size-before-span-construction
rubric: safety
severity: should_fix
---

**Rule.** When constructing `base::span` from raw buffer pointers, computed byte
sizes, or legacy C/Skia APIs, assert (`CHECK`) that all required raw pointer
arguments (such as `info` or `rowBytes`) are non-null before dereferencing them.
Verify that raw buffer pointers are valid rather than error sentinels (such as
`MAP_FAILED` or `nullptr` when size is non-zero, e.g.
`CHECK(row || size == 0)`), and assert that size calculations did not overflow
(`CHECK_NE(size, SIZE_MAX)`). Check pointers and sizes before constructing the
span under `UNSAFE_BUFFERS` or `UNSAFE_TODO` with a valid `// SAFETY:` comment.

**Why.** Constructing `base::span(ptr, size)` requires `ptr` to point to at
least `size` valid elements, and computing bounds often requires dereferencing
input pointer parameters. Unchecked null pointer dereferences cause crashes or
undefined behavior, while wrapping error sentinels like `MAP_FAILED` or
overflowed sizes (`SIZE_MAX`) violates memory safety preconditions and
invalidates `// SAFETY:` justifications.

## Examples

Bad:

```cpp
// Bad: Dereferencing pointer parameters without CHECK and omitting bounds checks
#define UNSAFE_PAINTCANVAS_TOP_LAYER_TO_BYTES_SPAN(arg_self, arg_info,        \
                                                   arg_row_bytes, arg_origin) \
  ([](auto&& self, SkImageInfo* info, size_t* rowBytes, SkIPoint* origin) {   \
    uint8_t* row = static_cast<uint8_t*>(                                     \
        self->accessTopLayerPixels(info, rowBytes, origin));                  \
    size_t size = info->computeByteSize(*rowBytes);                           \
    return UNSAFE_TODO(base::span<uint8_t>(row, size));                       \
  }(::base::spanification_internal::ToPointer(arg_self), arg_info,            \
    arg_row_bytes, arg_origin))

// Bad: Wrapping fallible call directly and checking span.data() against error sentinel
auto map_span = UNSAFE_BUFFERS(base::span<uint8_t>(
    static_cast<uint8_t*>(mmap(nullptr, map_size, prot, flags, fd, offset)),
    map_size));
if (map_span.data() == MAP_FAILED) {
  return false;
}
```

Good:

```cpp
// Good: Assert non-null pointer arguments, check size overflow, and assert row || size == 0
#define UNSAFE_PAINTCANVAS_TOP_LAYER_TO_BYTES_SPAN(arg_self, arg_info,        \
                                                   arg_row_bytes, arg_origin) \
  ([](auto&& self, SkImageInfo* info, size_t* rowBytes, SkIPoint* origin) {   \
    CHECK(info);                                                              \
    CHECK(rowBytes);                                                          \
    uint8_t* row = static_cast<uint8_t*>(                                     \
        self->accessTopLayerPixels(info, rowBytes, origin));                  \
    size_t size = info->computeByteSize(*rowBytes);                           \
    CHECK(row || size == 0);                                                  \
    CHECK_NE(size, SIZE_MAX);                                                 \
    return UNSAFE_TODO(base::span<uint8_t>(row, size));                       \
  }(::base::spanification_internal::ToPointer(arg_self), arg_info,            \
    arg_row_bytes, arg_origin))

// Good: Check raw pointer for MAP_FAILED before constructing span
auto* ptr = static_cast<uint8_t*>(
    mmap(nullptr, map_size, prot, flags, fd, offset));
if (ptr == MAP_FAILED) {
  return false;
}
// SAFETY: `ptr` points to `map_size` valid bytes from successful mmap.
auto map_span = UNSAFE_BUFFERS(base::span(ptr, map_size));
```
