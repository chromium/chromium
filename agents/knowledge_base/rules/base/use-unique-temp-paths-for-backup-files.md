---
name: base/use-unique-temp-paths-for-backup-files
rubric: correctness
severity: should_fix
---

**Rule.** Do not use static or deterministic filenames (such as appending a
fixed suffix like .replace_backup) for temporary or backup files in file
replacement and atomic write operations. Use CreateAndOpenTemporaryFileInDir to
generate a unique random .tmp path (calling DeleteOnClose(true) if an unopened
or non-existent path is required) to prevent collisions with leftover stale
files from interrupted operations and enable ImportantFileWriterCleaner cleanup.

**Why.** Fixed backup filenames collide when prior operations crash or fail
without cleaning up the backup file, causing subsequent file replacements to
fail. Generating unique .tmp filenames avoids collision races and ensures
orphaned files in watched directories can be purged automatically by
ImportantFileWriterCleaner.

## Examples

Bad:

```cpp
// Bad: Using a deterministic filename collides if a prior run left the backup file.
const FilePath backup_file_path =
    FilePath(to_path.value() + FILE_PATH_LITERAL(".replace_backup"));
if (!::ReplaceFile(to_path.value().c_str(), from_path.value().c_str(),
                   backup_file_path.value().c_str(), 0, NULL, NULL)) {
  return false;
}
```

Good:

```cpp
// Good: Generate a unique random .tmp file to avoid collisions and allow cleanup.
FilePath backup_path;
if (File temp_file =
        CreateAndOpenTemporaryFileInDir(to_path.DirName(), &backup_path);
    temp_file.IsValid()) {
  temp_file.DeleteOnClose(true);
} else {
  return false;
}
```
