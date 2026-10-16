---
name: general/do-not-treat-all-reparse-points-as-links
rubric: design
severity: should_fix
---

**Rule.** When determining whether a file or directory is a symbolic link or
directory junction on Windows, call canonical base::IsLink() instead of testing
only the FILE_ATTRIBUTE_REPARSE_POINT attribute bit or duplicating ad-hoc Win32
FindFirstFile and IsReparseTagNameSurrogate() queries across call sites.
base::IsLink() inspects the reparse tag to distinguish true name surrogate
redirection targets from non-surrogate reparse points (such as cloud storage
placeholders or deduplication tags).

**Why.** Windows uses FILE_ATTRIBUTE_REPARSE_POINT for non-redirection features
like cloud file placeholders and deduplication, so testing the attribute bit
alone misidentifies regular directories as links. Inlining custom reparse tag
inspections duplicates Win32 handle management boilerplate and invites subtle
behavioral inconsistencies across filesystem helpers.

## Examples

Bad:

```cpp
// Bad: testing FILE_ATTRIBUTE_REPARSE_POINT alone
if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) { ... }

// Bad: duplicating ad-hoc reparse tag inspection inline
WIN32_FIND_DATA find_data;
HANDLE handle = ::FindFirstFile(path.value().c_str(), &find_data);
if (handle != INVALID_HANDLE_VALUE) {
  bool is_link = IsReparseTagNameSurrogate(find_data.dwReserved0);
  ::FindClose(handle);
}
```

Good:

```cpp
// Good: use the canonical base::IsLink() helper
if (base::IsLink(path)) {
  // Handle symbolic link or directory junction.
}
```
