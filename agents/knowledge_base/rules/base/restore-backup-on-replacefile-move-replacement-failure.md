---
name: base/restore-backup-on-replacefile-move-replacement-failure
rubric: correctness
severity: should_fix
---

**Rule.** When invoking Win32 ::ReplaceFile with a backup file path, explicitly
handle ERROR_UNABLE_TO_MOVE_REPLACEMENT_2 upon failure by attempting to restore
the destination file from the backup file (e.g. via ::MoveFile).

**Why.** When Win32 ::ReplaceFile fails with ERROR_UNABLE_TO_MOVE_REPLACEMENT_2,
the destination file has already been renamed to the backup filename, but the
replacement file could not be moved into place. If the caller does not restore
the backup file back to the destination path, the original destination file is
left missing, causing data loss.

## Examples

Bad:

```cpp
if (!::ReplaceFile(to_path.value().c_str(), from_path.value().c_str(),
                   backup_path.value().c_str(),
                   REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
  // Bad: Moving from_path directly does not restore to_path if it was
  // already renamed to backup_path before ReplaceFile failed.
  ::MoveFile(from_path.value().c_str(), to_path.value().c_str());
}
```

Good:

```cpp
if (!::ReplaceFile(to_path.value().c_str(), from_path.value().c_str(),
                   backup_path.value().c_str(),
                   REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
  const DWORD error_code = ::GetLastError();
  if (error_code == ERROR_UNABLE_TO_MOVE_REPLACEMENT_2) {
    // Good: Restore to_path from the backup file to prevent data loss.
    ::MoveFile(backup_path.value().c_str(), to_path.value().c_str());
  }
}
```
