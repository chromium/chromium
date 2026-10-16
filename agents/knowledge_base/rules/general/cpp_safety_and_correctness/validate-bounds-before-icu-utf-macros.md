---
name: general/validate-bounds-before-icu-utf-macros
rubric: safety
severity: should_fix
---

**Rule.** Ensure the current offset or index is strictly within buffer bounds
(e.g., `index < length` or `index < span.size()`) before invoking ICU UTF
iteration macros such as `CBU8_NEXT`, `CBU16_NEXT`, `U8_NEXT`, or `U16_NEXT`. Do
not rely on the macro's length parameter to guard against an offset already at
or beyond the end of the buffer.

**Why.** ICU UTF decoding macros assume `index < length` as a precondition and
unconditionally evaluate `(s)[(i)++]` at the start of iteration. Invoking them
without first verifying that the offset is strictly less than the buffer length
causes immediate out-of-bounds memory reads on empty or exhausted buffers.

## Examples

Bad:

```cpp
bool ReadUnicodeCharacter(base::span<const char> src,
                          size_t* char_index,
                          base_icu::UChar32* code_point_out) {
  base_icu::UChar32 code_point;
  UNSAFE_TODO(CBU8_NEXT(base::as_bytes(src), *char_index, src.size(), code_point));
  *code_point_out = code_point;
  (*char_index)--;
  return IsValidCodepoint(code_point);
}
```

Good:

```cpp
bool ReadUnicodeCharacter(base::span<const char> src,
                          size_t* char_index,
                          base_icu::UChar32* code_point_out) {
  if (*char_index >= src.size()) {
    return false;
  }
  base_icu::UChar32 code_point;
  UNSAFE_TODO(CBU8_NEXT(base::as_bytes(src), *char_index, src.size(), code_point));
  *code_point_out = code_point;
  (*char_index)--;
  return IsValidCodepoint(code_point);
}
```
