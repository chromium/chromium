---
name: general/prefer-subspan-or-substr-over-unsafe-buffers-pointer-arithme
rubric: readability
severity: should_fix
---

**Rule.** When obtaining a sub-range, sub-view, or offset pointer from a
`base::span`, `base::HeapArray`, `std::string_view`, or contiguous container,
use safe member slicing methods like `.subspan()`, `.first()`, `.last()`,
`.substr()`, or `.subspan(offset).data()`. Do not use raw pointer arithmetic on
`.data()`/`.c_str()` wrapped in `UNSAFE_BUFFERS(...)`, and avoid address-of
element indexing (`&container[offset]`) when slicing sub-regions or obtaining
offset pointers.

**Why.** Using pointer arithmetic on `.data()` circumvents compiler
buffer-safety checks and requires `UNSAFE_BUFFERS` annotations. Address-of
indexing (`&container[offset]`) dereferences elements before taking their
address, failing assertions on past-the-end offsets (`offset == size`). Member
slicing methods like `.subspan()` provide safe, bounds-checked range access
across all valid offsets.

## Examples

Bad:

```cpp
// 1. Raw pointer arithmetic on .data() wrapped in UNSAFE_BUFFERS:
// SAFETY: The offset and length are guaranteed to be within bounds.
auto slice = UNSAFE_BUFFERS(std::string_view(view.data() + offset, length));
auto sub_span = UNSAFE_BUFFERS(base::span(span.data() + offset, count));

// 2. Taking address of an indexed element for sub-region offsets:
uintptr_t addr = reinterpret_cast<uintptr_t>(&heap_array[offset]);
```

Good:

```cpp
// 1. Safe member slicing for sub-views:
auto slice = view.substr(offset, length);
auto sub_span = span.subspan(offset, count);

// 2. Member subspan slicing and data pointer retrieval:
uintptr_t addr =
    reinterpret_cast<uintptr_t>(heap_array.subspan(offset).data());
```
