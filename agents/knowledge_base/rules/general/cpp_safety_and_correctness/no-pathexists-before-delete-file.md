---
name: general/no-pathexists-before-delete-file
rubric: safety
severity: should_fix
---

**Rule.** Do not call `base::PathExists()` to check for file or directory
existence before calling `base::DeleteFile()` or
`base::DeletePathRecursively()`. Invoke the deletion function directly.

**Why.** Checking path existence before deletion introduces a redundant
filesystem query and a time-of-check to time-of-use (TOCTOU) race condition. In
Chromium, `base::DeleteFile()` and `base::DeletePathRecursively()` already
succeed when the target does not exist.

## Examples

Bad:

```cpp
if (base::PathExists(backup_file_path)) {
  base::DeleteFile(backup_file_path);
}
```

Good:

```cpp
base::DeleteFile(backup_file_path);
```
