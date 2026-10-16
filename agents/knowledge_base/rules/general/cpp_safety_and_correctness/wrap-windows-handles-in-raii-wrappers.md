---
name: general/wrap-windows-handles-in-raii-wrappers
rubric: safety
severity: should_fix
---

**Rule.** When opening, creating, or receiving Windows file or kernel HANDLEs
(e.g., via ::CreateFile, ::CreateEvent, or IPC), wrap them immediately in RAII
wrappers—such as base::File for file handles or base::win::ScopedHandle for
general system handles—rather than holding raw HANDLEs and manually calling
::CloseHandle.

**Why.** Managing raw HANDLE values and relying on manual ::CloseHandle() calls
is error-prone, risking handle leaks on early exits, unexpected errors, or
subsequent refactorings. Using base::File and base::win::ScopedHandle ensures
automatic cleanup on all exit paths, prevents accidental pseudo-handle misuse,
and clarifies object ownership.

## Examples

Bad:

```cpp
HANDLE handle = ::CreateFile(
    file_path.value().c_str(), FILE_READ_ATTRIBUTES,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
    nullptr);
if (handle == INVALID_HANDLE_VALUE) {
  return false;
}
FILE_ATTRIBUTE_TAG_INFO info;
bool success = ::GetFileInformationByHandleEx(handle, FileAttributeTagInfo,
                                              &info, sizeof(info));
::CloseHandle(handle);
```

Good:

```cpp
base::File file(::CreateFile(
    file_path.value().c_str(), FILE_READ_ATTRIBUTES,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
    nullptr));
if (!file.IsValid()) {
  return false;
}
FILE_ATTRIBUTE_TAG_INFO info;
bool success = ::GetFileInformationByHandleEx(
    file.GetPlatformFile(), FileAttributeTagInfo, &info, sizeof(info));
```
