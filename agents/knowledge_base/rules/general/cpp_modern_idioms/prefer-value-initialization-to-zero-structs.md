---
name: general/prefer-value-initialization-to-zero-structs
rubric: readability
severity: should_fix
---

**Rule.** When zero-initializing or resetting a plain struct, aggregate, or
out-parameter, use standard C++ value-initialization (such as `*out = {}` or
`*out = Type{}`) rather than manually clearing its memory as a byte range with
`std::ranges::fill(base::byte_span_from_ref(...), 0)`, `memset()`, or
`ZeroMemory()`. Reserve byte-level filling for raw memory buffers and byte
arrays.

**Why.** Treating a high-level struct as a raw byte span adds unnecessary
complexity and verbosity. Standard C++ value-initialization is type-safe,
declarative, and allows the compiler to generate clean, optimized
zero-initialization code without low-level byte manipulation.

## Examples

Bad:

```cpp
// Avoid wrapping plain structs in byte spans to zero-fill them:
std::ranges::fill(base::byte_span_from_ref(*exploded), 0);

// Avoid C-style memory-clearing functions on plain structs:
memset(&info, 0, sizeof(info));
```

Good:

```cpp
// Prefer standard C++ value-initialization:
*exploded = Exploded();
// Or using empty braces:
*exploded = {};
info = {};
```
