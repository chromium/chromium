---
name: general/deprecate-unsafe-buffer-overloads-in-favor-of-span
rubric: safety
severity: should_fix
---

**Rule.** When declaring or defining functions that accept contiguous buffers,
pointer arrays (such as `char**`), or array segments, accept `base::span<T>`,
`base::span<T*>`, or `std::string_view` instead of raw pointers and size pairs.
When modernizing existing APIs, migrate callers across the codebase and delete
the legacy raw-pointer overload completely whenever feasible. If a legacy
raw-pointer overload must be temporarily retained for backward compatibility,
mark it with `UNSAFE_BUFFER_USAGE` and add a comment indicating it is deprecated
and directing callers to the safe replacement.

**Why.** Raw pointers and pointer-and-size pairs discard buffer bounds and size
information, forcing callers and callees to operate without bounds validation.
Accepting `base::span` or `std::string_view` preserves bounds, enables runtime
bounds checks, and provides a memory-safe interface. Deleting unneeded legacy
overloads prevents unsafe technical debt from lingering, while
`UNSAFE_BUFFER_USAGE` annotations and deprecation comments ensure remaining
legacy call sites stay audited and guide developers toward safe overloads.

## Examples

Bad:

```cpp
// 1. Raw array / pointer parameter loses bounds information:
BASE_EXPORT void SetEnvironment(char** env);

// 2. Retaining legacy raw-pointer overload without deprecation note:
UNSAFE_BUFFER_USAGE BASE_EXPORT bool ParseData(const uint8_t* data, size_t size);

// 3. Retaining deprecated wrapper when callers could be migrated cleanly:
// Deprecated. Use string_view version instead.
UNSAFE_BUFFER_USAGE BASE_EXPORT bool ReadUnicodeCharacter(
    const char* src,
    size_t src_len,
    size_t* char_index,
    base_icu::UChar32* code_point_out);
```

Good:

```cpp
// 1. Bounds-checked span preserves size and enables safe iteration:
BASE_EXPORT void SetEnvironment(base::span<char*> env);

// 2. Deprecation comment provided when temporary backward compatibility is required:
// Deprecated. Use span version instead.
UNSAFE_BUFFER_USAGE BASE_EXPORT bool ParseData(const uint8_t* data, size_t size);
BASE_EXPORT bool ParseData(base::span<const uint8_t> data);

// 3. Migrate callers and delete deprecated raw-pointer overload entirely:
BASE_EXPORT bool ReadUnicodeCharacter(std::string_view src,
                                      size_t* char_index,
                                      base_icu::UChar32* code_point_out);
```
