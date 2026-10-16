---
name: general/document-loadlibraryex-search-flags-with-full-path
rubric: readability
severity: nit
---

**Rule.** When invoking Windows LoadLibraryEx (or LoadLibraryExW) with an
absolute or fully qualified path alongside LOAD_LIBRARY_SEARCH\_\* flags (such
as LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR or LOAD_LIBRARY_SEARCH_SYSTEM32), add an
explanatory code comment. Clarify that while the primary DLL is loaded directly
from the specified path, the search flags control how the library's downstream
dependencies are resolved.

**Why.** When LoadLibraryEx receives an absolute path, the target module is
loaded from that path directly, making search flags appear redundant or
contradictory to reviewers. Documenting that search flags govern dependency
resolution prevents confusion and avoids accidental removal of critical DLL
search path constraints.

## Examples

Bad:

```cpp
base::FilePath dbghelp_path = GetDllPath().DirName().Append(L"dbghelp.dll");
dbghelp_handle = ::LoadLibraryExW(
    dbghelp_path.value().c_str(), nullptr,
    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
```

Good:

```cpp
// The search flags ensure dbghelp's dependencies are loaded only from its
// directory or the system directory.
base::FilePath dbghelp_path = GetDllPath().DirName().Append(L"dbghelp.dll");
dbghelp_handle = ::LoadLibraryExW(
    dbghelp_path.value().c_str(), nullptr,
    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
```
