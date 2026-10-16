---
name: general/prefer-span-copy-from-over-memcpy
rubric: safety
severity: blocking
---

**Rule.** Replace raw memcpy() and memmove() calls with base::span::copy_from()
(or std::ranges::fill() for memset()), ensuring compile-time or runtime
verification that source and destination buffers match in size. When
constructing spans from raw pointers at boundaries, wrap the span construction
in UNSAFE_BUFFERS(...) with a documented // SAFETY: comment.

**Why.** Raw memcpy() and memmove() bypass bounds safety by relying on unchecked
pointer-and-size arguments, creating out-of-bounds read/write hazards.
base::span::copy_from() verifies span extents match before copying and optimizes
down to memmove where safe.

## Examples

Bad:

```cpp
std::array<TlsVectorEntry, kThreadLocalStorageSize> stack_tls;
memcpy(stack_tls.data(), tls_data,
       base::SpanificationSizeofForStdArray(stack_tls));
```

Good:

```cpp
std::array<TlsVectorEntry, kThreadLocalStorageSize> stack_tls;
// SAFETY: tls_data was allocated with kThreadLocalStorageSize elements.
auto data_span = UNSAFE_BUFFERS(base::span(tls_data, kThreadLocalStorageSize));
base::span(stack_tls).copy_from(data_span);
```
