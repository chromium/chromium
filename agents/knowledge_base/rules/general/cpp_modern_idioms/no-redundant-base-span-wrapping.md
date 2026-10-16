---
name: general/no-redundant-base-span-wrapping
rubric: readability
severity: nit
---

**Rule.** Do not explicitly wrap arguments in base::span(...) or
base::span<T>(...) at call sites when the callee already accepts matching string
views, contiguous containers, or parameters that implicitly construct
base::span. Pass the variable or expression directly to avoid unnecessary
boilerplate.

**Why.** base::span and std::string_view implicitly construct from matching
string, vector, and array types. Explicitly constructing base::span wrappers at
call sites adds visual clutter without providing additional type safety or
clarity.

## Examples

Bad:

```cpp
// Redundant explicit span wrapping when callee accepts std::u16string_view or base::span.
PrepareForUTF8Output(base::span<const char16_t>(utf16), &result);
ConvertUnicode(base::span(utf16), &result, &adjustments);
```

Good:

```cpp
// Pass argument directly to let implicit conversion or parameter matching apply.
PrepareForUTF8Output(utf16, &result);
ConvertUnicode(utf16, &result, &adjustments);
```
