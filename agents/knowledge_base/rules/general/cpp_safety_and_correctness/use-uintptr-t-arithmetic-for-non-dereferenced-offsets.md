---
name: general/use-uintptr-t-arithmetic-for-non-dereferenced-offsets
rubric: safety
severity: should_fix
---

**Rule.** When calculating byte offsets, addresses, or buffer lengths without
dereferencing memory, perform integer arithmetic on addresses cast to uintptr_t
instead of raw pointer arithmetic. Avoid wrapping raw pointer arithmetic in
UNSAFE_BUFFERS or UNSAFE_TODO when no memory dereference occurs.

**Why.** Raw pointer addition and subtraction trigger Clang's
-Wunsafe-buffer-usage warnings even when pointers are never dereferenced.
Converting addresses to uintptr_t for integer arithmetic safely computes offsets
and sizes without requiring UNSAFE_BUFFERS macro suppressions or exposing raw
pointer operations.

## Examples

Bad:

```cpp
const uint8_t* headers_end =
    reinterpret_cast<const uint8_t*>(last_section_header) +
    sizeof(*last_section_header);
size_t header_size = headers_end - data_file_.data();
```

Good:

```cpp
const uintptr_t last_section_header_addr =
    reinterpret_cast<uintptr_t>(last_section_header);
const uintptr_t headers_end_addr =
    last_section_header_addr + sizeof(*last_section_header);
const uintptr_t data_start_addr =
    reinterpret_cast<uintptr_t>(data_file_.data());
size_t header_size = headers_end_addr - data_start_addr;
```
