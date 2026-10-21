---
name: base/distinguish-hidden-staging-files-from-temp-entries
rubric: correctness
severity: should_fix
---

**Rule.** When implementing or updating temporary file and directory creation
utilities, distinguish between staging files created in arbitrary destination
directories (such as for atomic write-and-replace) and scratch entries created
in dedicated temporary directories. Ensure staging files in arbitrary
directories are hidden (e.g., prefixed with a leading dot on POSIX) to avoid
surfacing incomplete files to users or file watchers, while scratch files and
directories in designated temporary locations remain unhidden for discovery and
cleanup.

**Why.** Atomic file write patterns create temporary staging files alongside
target files in user-visible or indexed directories before replacing the
destination. Leaving these staging files unhidden exposes incomplete files to
users, file managers, and filesystem monitors. Conversely, entries inside
dedicated system temporary directories should remain visible so administrators,
users, and cleanup tools can easily inspect and remove orphaned scratch files.

## Examples

Bad:

```cpp
// Unconditionally omitting the hidden prefix unhides staging files
// created in arbitrary user-visible directories during atomic writes.
FilePath FormatTemporaryFileName(FilePath::StringViewType identifier) {
  return FilePath(StrCat({prefix, ".", identifier}));
}

*path = directory.Append(GetTempTemplate(name_prefix));
```

Good:

```cpp
// Distinguish hidden staging files from unhidden temp directory entries:
FilePath FormatTemporaryFileName(FilePath::StringViewType identifier,
                                 bool hidden) {
  return FilePath(StrCat({hidden ? "." : "", prefix, ".", identifier}));
}

// Staging files in arbitrary directories remain hidden (dot-prefixed):
*path = directory.Append(GetTempTemplate(name_prefix, /*hidden=*/true));

// Scratch directories in dedicated temp directories remain visible:
return CreateTemporaryDirInDirImpl(
    tmpdir, GetTempTemplate(prefix, /*hidden=*/false), new_temp_path);
```
