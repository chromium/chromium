---
name: general/document-rationale-for-non-obvious-fallback-paths
rubric: readability
severity: nit
---

**Rule.** When implementing a non-obvious fallback path, secondary query, or
retry attempt after a primary operation fails, add an inline comment explaining
why the fallback is attempted and what specific failure modes, edge cases, or
platform restrictions (such as sharing violations or access-denied errors) it
accommodates.

**Why.** Secondary fallback paths and alternate queries can appear redundant,
unmotivated, or ad-hoc without context. Explaining the expected failure modes
and recovery rationale helps reviewers understand why the fallback exists and
prevents future maintainers from accidentally removing necessary error
accommodation.

## Examples

Bad:

```cpp
if (!file.IsValid()) {
  WIN32_FIND_DATA find_data;
  HANDLE find_handle = ::FindFirstFile(file_path.value().c_str(), &find_data);
  if (find_handle != INVALID_HANDLE_VALUE) {
    ::FindClose(find_handle);
    return IsReparseTagNameSurrogate(find_data.dwReserved0);
  }
  return false;
}
```

Good:

```cpp
if (!file.IsValid()) {
  // Best-effort fallback for cases where CreateFile fails (e.g., sharing
  // violation or access denied).
  WIN32_FIND_DATA find_data;
  HANDLE find_handle = ::FindFirstFile(file_path.value().c_str(), &find_data);
  if (find_handle != INVALID_HANDLE_VALUE) {
    ::FindClose(find_handle);
    return IsReparseTagNameSurrogate(find_data.dwReserved0);
  }
  return false;
}
```
