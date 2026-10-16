---
name: general/do-not-rely-on-makeabsolutefilepath-for-existence
rubric: correctness
severity: blocking
---

**Rule.** Do not rely on base::MakeAbsoluteFilePath() to verify that a file or
directory exists. On POSIX platforms, MakeAbsoluteFilePath() fails on
non-existent paths via realpath(), but on Windows it succeeds via \_fullpath().
If target existence is required, explicitly check base::PathExists().

**Why.** base::MakeAbsoluteFilePath() exhibits platform-inconsistent behavior on
non-existent targets: POSIX returns an empty FilePath if the path does not
exist, whereas Windows normalizes and returns the absolute path regardless of
existence. Relying solely on MakeAbsoluteFilePath() returning empty will fail to
detect missing files on Windows.

## Examples

Bad:

```cpp
base::FilePath resolved = base::MakeAbsoluteFilePath(path);
if (resolved.empty()) {
  ReportError();
}
```

Good:

```cpp
base::FilePath resolved = base::MakeAbsoluteFilePath(path);
if (resolved.empty() || !base::PathExists(resolved)) {
  ReportError();
}
```
