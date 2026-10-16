---
name: general/update-counterpart-platform-implementations-for-cross-platfo
rubric: correctness
severity: should_fix
---

**Rule.** When fixing a bug, handling an edge case, or updating logic in a
platform-specific implementation of a cross-platform API (such as in
`*_posix.cc`, `*_win.cc`, `*_apple.mm`, `*_android.cc`, or `*_linux.cc`),
inspect all sibling platform-specific files implementing that API and apply the
corresponding fix to all counterpart implementations. Do not leave counterpart
targets with unpatched logic bugs or divergent behavior.

**Why.** Cross-platform APIs often split implementations across
platform-specific files or `#if BUILDFLAG(IS_...)` blocks with duplicated or
parallel logic. Fixing an edge case or defect in only one platform
implementation leaves other platforms unpatched and introduces subtle
cross-platform behavioral inconsistencies.

## Examples

Bad:

```cpp
// base/files/file_util_posix.cc
bool GetTempDir(FilePath* path) {
  const char* tmp = getenv("TMPDIR");
  if (tmp && tmp[0]) {
    *path = FilePath(tmp);
    return true;
  }
  return false;
}

// base/files/file_util_apple.mm - counterpart was overlooked
bool GetTempDir(base::FilePath* path) {
  const char* env_tmpdir = getenv("MAC_CHROMIUM_TMPDIR");
  if (env_tmpdir) {
    *path = base::FilePath(env_tmpdir);
    return true;
  }
  return false;
}
```

Good:

```cpp
// base/files/file_util_posix.cc
bool GetTempDir(FilePath* path) {
  const char* tmp = getenv("TMPDIR");
  if (tmp && tmp[0]) {
    *path = FilePath(tmp);
    return true;
  }
  return false;
}

// base/files/file_util_apple.mm - counterpart updated
bool GetTempDir(base::FilePath* path) {
  const char* env_tmpdir = getenv("MAC_CHROMIUM_TMPDIR");
  if (env_tmpdir && env_tmpdir[0]) {
    *path = base::FilePath(env_tmpdir);
    return true;
  }
  return false;
}
```
