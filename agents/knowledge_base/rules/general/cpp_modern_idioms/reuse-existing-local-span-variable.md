---
name: general/reuse-existing-local-span-variable
rubric: readability
severity: nit
---

**Rule.** When a container, string, or object has already been converted to a
local base::span or std::string_view variable in the current scope, reuse the
existing span or view variable in subsequent calls, copies, or queries. Avoid
redundantly re-invoking conversion helpers (such as base::as_byte_span(),
base::as_chars(), base::as_string_view(), or explicit span/view constructors) on
the original source object.

**Why.** Redundantly re-converting a container or object into a span or view
multiple times in the same scope duplicates conversion logic, introduces visual
clutter, and can obscure whether subsequent operations view the same slice or a
different sub-range.

## Examples

Bad:

```cpp
auto target_str_span = base::as_byte_span(target_str);
dest_span.first(target_str_span.size())
    .copy_from(base::as_byte_span(target_str));
```

Good:

```cpp
auto target_str_span = base::as_byte_span(target_str);
dest_span.first(target_str_span.size())
    .copy_from(target_str_span);
```
