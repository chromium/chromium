---
name: general/prefer-replacefile-over-move-for-temp-files
rubric: readability
severity: should_fix
---

**Rule.** When replacing or updating a destination file with a temporary or
staging file, use `base::ReplaceFile` instead of `base::Move`. On Windows,
`base::ReplaceFile` preserves target file attributes, security descriptors, and
permissions upon replacement.

**Why.** `base::Move` does not preserve target file attributes or security
permissions on Windows. `base::ReplaceFile` uses platform replacement APIs to
maintain existing target file attributes and permissions.

## Examples

Bad:

```cpp
if (!base::Move(temp_file_path, target_file_path)) {
  return false;
}
```

Good:

```cpp
if (!base::ReplaceFile(temp_file_path, target_file_path, /*error=*/nullptr)) {
  return false;
}
```
