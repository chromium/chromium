---
name: general/prefer-byte-span-from-ref-over-unsafe-reinterpret-cast
rubric: readability
severity: should_fix
---

**Rule.** When converting a single object or primitive to a byte span, use
`base::byte_span_from_ref()` rather than composing
`as_bytes(span_from_ref(...))` / `as_writable_bytes(span_from_ref(...))` or
casting pointers. When converting a contiguous container, string, or range to a
byte span (or querying its byte size), use `base::as_byte_span()` or
`base::as_writable_byte_span()`. When converting an existing span's element type
to `uint8_t` or `char`, use `base::as_bytes()` / `base::as_writable_bytes()` or
`base::as_chars()` / `base::as_writable_chars()`. When constructing a
`std::string_view` (or `std::u16string_view` / `std::wstring_view`) from a byte
or character span/container, use `base::as_string_view()` (from
`base/strings/string_view_util.h`). Avoid casting pointers with
`reinterpret_cast` under `UNSAFE_BUFFERS`, constructing spans or string views
from raw `.data()`/`.c_str()` pointers, or manually computing byte lengths with
`size() * sizeof(...)`.

**Why.** `base::byte_span_from_ref()`, `base::as_byte_span()`,
`base::as_bytes()`, `base::as_chars()`, and `base::as_string_view()` provide
type-safe, bounds-checked views. Using dedicated helpers avoids verbose or
redundant compositions like `as_writable_bytes(span_from_ref(obj))`, prevents
incorrect size computations (`size() * sizeof(...)`), eliminates raw pointer
extractions with `reinterpret_cast` or manual pointer-size constructors, and
avoids unnecessary `UNSAFE_BUFFERS` annotations across the codebase.

## Examples

Bad:

```cpp
// Bad: raw pointer reinterpret_cast and manual byte arithmetic
const size_t bytes_len = target_str.size() * sizeof(wchar_t);
auto span1 = UNSAFE_BUFFERS(
    base::span(reinterpret_cast<const uint8_t*>(target_str.data()), bytes_len));

mode_t masked_mode = stat_buf.st_mode & ~S_IFMT;
UNSAFE_BUFFERS(*(reinterpret_cast<mode_t*>(buffer.data())) = masked_mode);

// Bad: redundant composition to obtain byte span of single object
Pickle::Header header;
as_writable_bytes(span_from_ref(header)).copy_from_nonoverlapping(data.first(sizeof(header)));

// Bad: converting byte span to char span or string view via reinterpret_cast
UNSAFE_BUFFERS(
    base::span<const char> chars(reinterpret_cast<const char*>(bytes.data()),
                                 bytes.size()));
std::string_view bytes_view(reinterpret_cast<const char*>(bytes.data()),
                            bytes.size());
```

Good:

```cpp
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"

// Good: safe byte spans via base::as_byte_span and base::byte_span_from_ref
base::span<const uint8_t> span1 = base::as_byte_span(target_str);
const size_t bytes_len = span1.size();

mode_t masked_mode = stat_buf.st_mode & ~S_IFMT;
auto mask_span = base::byte_span_from_ref(masked_mode);
buffer.first(mask_span.size()).copy_from(mask_span);

// Good: single-object byte span without redundant composition
Pickle::Header header;
base::byte_span_from_ref(header).copy_from_nonoverlapping(data.first(sizeof(header)));

// Good: safe char span and string view conversions
base::span<const char> chars = base::as_chars(bytes);
std::string_view bytes_view = base::as_string_view(bytes);
```
