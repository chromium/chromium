---
name: general/refer-to-codebase-symbols-by-name-not-urls-in-comments
rubric: readability
severity: nit
---

**Rule.** When referencing classes, methods, or symbols in comments, refer to
identifiers directly by name (wrapped in backticks) rather than embedding
mutable codesearch URLs. When embedding source.chromium.org URLs for external or
third-party source references, format them as stable permalinks including both
the commit hash (drc=...) and line number (l=...), avoiding unlanded paths on
main that yield dead links.

**Why.** Symbols referenced by name in backticks remain robust against
refactoring and searchable across tools. When source URLs are necessary,
omitting the commit hash or line number allows links to drift or break over
time, and linking to unlanded paths causes 404 errors.

## Examples

Bad:

```cpp
// Bad: Embedding codesearch links for internal symbols instead of backticked names:
// https://crsrc.org/c/base/message_loop/message_pump_apple.mm?q=MessagePumpCFRunLoopBase::Run

// Bad: Source link missing commit hash and line number, or referencing unlanded paths:
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/harfbuzz/src/src/hb-buffer.h
#define UNSAFE_HB_BUFFER_GET_GLYPH_INFOS(arg_buffer, arg_length)
```

Good:

```cpp
// Good: Refer directly to codebase entities by name:
// In a standard Chromium pump, we use `MessagePumpCFRunLoopBase::Run()`.

// Good: Stable permalink with commit hash and line number:
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/harfbuzz-ng/src/src/hb-buffer.h;drc=ea6a172f84f2cbcfed803b5ae71064c7afb6b5c2;l=647
#define UNSAFE_HB_BUFFER_GET_GLYPH_INFOS(arg_buffer, arg_length)
```
