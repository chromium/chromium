---
name: general/do-not-assume-pickle-unowned-buffer-alignment
rubric: correctness
severity: blocking
---

**Rule.** Ensure byte storage and raw byte buffer casts satisfy runtime
alignment requirements:

1. When declaring or refactoring byte storage (such as char[], uint8_t[], or
   std::array) that is reinterpreted as structured types or populated by
   syscalls, preserve or specify alignas(...) to match the alignment
   requirements of the structured type.
2. When casting raw byte buffers or constructing non-owning views (such as
   span<const T>, std::u16string_view, or Pickle payloads), do not rely on
   static_assert(alignof(T)) to assume runtime buffer alignment. Dynamically
   verify pointer alignment with base::IsAligned(ptr, alignof(T)) or copy
   elements using unaligned-safe readers (such as base::SpanReader).
3. When deserialization or buffer helpers deliberately restrict zero-copy
   extraction to byte spans (span<const uint8_t>) rather than arbitrary types T,
   document the alignment restriction in comments.

**Why.** Byte arrays default to 1-byte alignment. Reinterpreting unaligned byte
buffers as typed pointers or constructing typed views without guaranteed
alignment causes undefined behavior in C++ and can trigger hardware faults (such
as SIGBUS on alignment-sensitive architectures) or performance regressions.
Explicit alignas(...) ensures storage alignment at definition sites,
base::IsAligned() dynamically guards pointer casts, unaligned readers
(base::SpanReader) avoid unsafe casts altogether, and documenting alignment
constraints clarifies why zero-copy APIs are restricted to byte spans.

## Examples

Bad:

```cpp
// Flawed: refactoring buffer dropped alignas needed for structured access.
std::array<unsigned char, 512> buf_;
...
linux_dirent* dirent = reinterpret_cast<linux_dirent*>(&buf_[offset_]);

// Flawed: static_assert only checks type traits, not runtime buffer alignment.
static_assert(alignof(T) <= sizeof(uint32_t));
UNSAFE_BUFFERS(
    *result = span<const T>(reinterpret_cast<const T*>(bytes.data()), len));

// Flawed: assumes unowned bytes or Pickle payload is aligned to char16_t.
*view = std::u16string_view(
    reinterpret_cast<const char16_t*>(iter.payload()), len);

// Flawed: restricts deserialization to bytes without explaining why T is unsupported.
[[nodiscard]] bool ReadLengthDelimitedArrayAndAlign(
    SpanReader<const uint8_t>& reader,
    span<const uint8_t>& result) {
  return ReadBytesAndAlign(reader, len, result);
}
```

Good:

```cpp
// Safe: preserves explicit alignment on container storage.
alignas(linux_dirent) std::array<unsigned char, 512> buf_ = {};
...
linux_dirent* dirent = reinterpret_cast<linux_dirent*>(&buf_[offset_]);

// Safe: dynamically verifies runtime pointer alignment before casting.
CHECK(base::IsAligned(bytes.data(), alignof(T)));
UNSAFE_BUFFERS(
    *result = span<const T>(reinterpret_cast<const T*>(bytes.data()), len));

// Safe: reads and copies elements without assuming pointer alignment.
result_str->clear();
base::SpanReader chars(bytes);
for (uint16_t ch; chars.ReadU16NativeEndian(ch);) {
  result_str->push_back(ch);
}

// Safe: documents why zero-copy span extraction cannot support arbitrary T.
// This supports bytes but not arbitrary types because the start of the data
// doesn't necessarily correspond to a suitably aligned pointer.
[[nodiscard]] bool ReadLengthDelimitedArrayAndAlign(
    SpanReader<const uint8_t>& reader,
    span<const uint8_t>& result) {
  return ReadBytesAndAlign(reader, len, result);
}
```
