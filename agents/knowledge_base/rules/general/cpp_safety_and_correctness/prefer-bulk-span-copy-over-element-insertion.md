---
name: general/prefer-bulk-span-copy-over-element-insertion
rubric: safety
severity: should_fix
---

**Rule.** When transferring raw bytes, serialized streams, or trivially copyable
elements into a resizable container (such as std::vector), resize the
destination container and copy elements in bulk using base::span::copy_from() or
copy_from_nonoverlapping() instead of iterating element-by-element with
push_back() or emplace_back(). When sizing std::string or std::u16string to
immediately populate it from an external buffer or span, prefer
.resize_and_overwrite() over calling .resize() followed by copying.

**Why.** Bulk copying via base::span::copy_from() or copy_from_nonoverlapping()
lowers to optimized memmove or memcpy operations and avoids the CPU overhead of
repetitive method calls, branch checks, and potential container reallocations.
For string types, calling .resize() value-initializes (zero-fills) every
character before it is overwritten; .resize_and_overwrite() allocates
uninitialized buffer capacity and writes into it directly, eliminating the
redundant zero-initialization pass.

## Examples

Bad:

```cpp
// Bad: Reading element-by-element in a loop with push_back.
result->clear();
result->reserve(len);
base::SpanReader reader(byte_span);
for (uint16_t ch; reader.ReadU16NativeEndian(ch);) {
  result->push_back(ch);
}

// Bad: Calling resize() zero-fills the string buffer before it is overwritten.
result->resize(len);
base::as_writable_bytes(base::span(*result))
    .copy_from_nonoverlapping(byte_span);
```

Good:

```cpp
// Good: Container bulk-copying lowers directly to optimized memcpy.
vec.resize(len);
base::span(vec).copy_from_nonoverlapping(element_span);

// Good: resize_and_overwrite populates strings directly without zero-fill.
result->resize_and_overwrite(len, [&](char16_t* p, size_t n) {
  // SAFETY: `resize_and_overwrite` ensures `p` points to `n` elements.
  base::as_writable_bytes(UNSAFE_BUFFERS(base::span(p, n)))
      .copy_from_nonoverlapping(byte_span);
  return n;
});
```
