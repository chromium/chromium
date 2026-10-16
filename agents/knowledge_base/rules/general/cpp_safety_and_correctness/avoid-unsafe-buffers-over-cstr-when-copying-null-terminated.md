---
name: general/avoid-unsafe-buffers-over-cstr-when-copying-null-terminated
rubric: correctness
severity: should_fix
---

**Rule.** When copying a string into a buffer that requires null termination,
use safe container conversions like base::as_byte_span() and .copy_from() rather
than constructing UNSAFE_BUFFERS spans over .c_str(). Explicitly write the
terminating null in the destination byte span; for multi-byte or wide character
strings (such as wchar_t, char16_t, or WCHAR), write the full multi-byte null
terminator (sizeof(CharT) zero bytes, e.g. using
std::ranges::fill(dest_span.subspan(offset, sizeof(CharT)), 0)) rather than
writing only a single '\\0' byte.

**Why.** Constructing base::span from str.c_str() with a computed length to
include the trailing null byte requires unsafe pointer arithmetic and casts
under UNSAFE_BUFFERS. Furthermore, assigning only a single 1-byte '\\0' to
null-terminate wide-character strings leaves trailing null bytes uninitialized,
causing buffer overreads or corrupt strings when read as wide characters.

## Examples

Bad:

```cpp
// Variant 1: Unsafe span construction over .c_str()
auto source_span = UNSAFE_BUFFERS(
    base::span(reinterpret_cast<const uint8_t*>(str.c_str()),
               size_with_null_bytes));
dest_span.first(size_with_null_bytes).copy_from(source_span);

// Variant 2: Incomplete 1-byte null terminator for wide string buffer
auto wide_span = base::as_byte_span(wide_str);
dest_span.first(wide_span.size()).copy_from(wide_span);
dest_span[wide_span.size()] = '\0';  // Leaves remaining sizeof(wchar_t)-1 bytes uninitialized
```

Good:

```cpp
// Variant 1: Narrow char string
auto str_span = base::as_byte_span(str);
dest_span.first(str_span.size()).copy_from(str_span);
dest_span[str_span.size()] = '\0';

// Variant 2: Wide char string (e.g. std::wstring or WCHAR buffer)
auto wide_span = base::as_byte_span(wide_str);
dest_span.first(wide_span.size()).copy_from(wide_span);
std::ranges::fill(dest_span.subspan(wide_span.size(), sizeof(wchar_t)), 0);
```
