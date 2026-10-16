---
name: general/const-auto-for-flag-combinations
rubric: readability
severity: should_fix
---

**Rule.** Prefer `const auto` for local variables holding composite bitwise
flags, options, or enum masks forwarded directly to APIs accepting those types,
rather than specifying an explicit integer type (such as `int` or `uint32_t`).

**Why.** Bitwise flag constants are often defined as scoped enums or unsigned
integer constants. Using `const auto` guarantees immutability while keeping
callers type-agnostic and resilient against signed/unsigned representation
mismatches with underlying API types.

## Examples

Bad:

```cpp
int file_flags =
    File::FLAG_OPEN_ALWAYS | File::FLAG_WRITE | File::FLAG_WIN_SHARE_DELETE;
File first_file(file_path, file_flags);
```

Good:

```cpp
const auto file_flags =
    File::FLAG_OPEN_ALWAYS | File::FLAG_WRITE | File::FLAG_WIN_SHARE_DELETE;
File first_file(file_path, file_flags);
```
