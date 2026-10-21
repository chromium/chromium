---
name: base/test-capability-restrictions-of-file-handle-flags
rubric: testing
severity: should_fix
---

**Rule.** When implementing or extending cross-platform file abstractions that
rely on OS file handle flags (such as `base::File::FLAG_APPEND`) to restrict
capabilities or enforce access boundaries, include unit tests verifying that
restricted operations (such as seeking or writing at arbitrary offsets via
`File::Write`) cannot overwrite existing data across all supported platforms.

**Why.** Underlying operating systems handle file handle flags and capability
lockdowns differently (e.g. POSIX open/lseek/pwrite vs Windows
CreateFile/SetFilePointer/WriteFile). Explicitly unit testing that seeking and
offset writes do not clobber existing data ensures capability restrictions and
append-only security invariants behave consistently across platforms.

## Examples

Bad:

```cpp
// Support FLAG_APPEND with FLAG_CREATE_ALWAYS in base::File:
if (flags & FLAG_CREATE_ALWAYS) {
  DCHECK(!open_flags);
  DCHECK(flags & (FLAG_WRITE | FLAG_APPEND));
  open_flags = O_CREAT | O_TRUNC;
}
// Flaw: No cross-platform unit test verifies whether seeking or offset writes
// (File::Write) are properly locked down or can overwrite existing data.
```

Good:

```cpp
TEST(FileTest, AppendEffectiveness) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath file_path = temp_dir.GetPath().AppendASCII("append_file_cant_seek");
  File file(file_path, File::FLAG_CREATE | File::FLAG_APPEND);
  ASSERT_TRUE(file.IsValid());

  file.Write(0, as_byte_span("test"));

  // Attempt to Seek should fail or have no effect due to FLAG_APPEND.
  std::ignore = file.Seek(File::FROM_BEGIN, /*offset=*/0);

  // Positional/offset writes should append rather than overwrite existing data.
  file.Write(/*offset=*/0, as_byte_span("bar"));

  file.Close();
  std::string actual_contents;
  EXPECT_TRUE(ReadFileToString(file_path, &actual_contents));
  EXPECT_EQ("testbar", actual_contents);
}
```
