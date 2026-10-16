---
name: general/avoid-redundant-string-view-when-filepath-available
rubric: readability
severity: nit
---

**Rule.** Do not declare intermediate view objects (such as std::string_view,
std::wstring_view, or base::span) solely to inspect properties like .size(),
.length(), or .empty() when the underlying container, string, or base::FilePath
already directly provides them. Query the source container or
path.value().size() directly.

**Why.** Creating intermediate view variables solely to query size or emptiness
adds unnecessary local variable clutter and boilerplate. Standard containers,
std::string, and base::FilePath already expose these methods directly without
needing wrapper objects.

## Examples

Bad:

```cpp
// Bad: Creating intermediate views solely to query size or length.
const std::wstring_view target_view = target_str;
const size_t size_with_null = (target_view.size() + 1) * sizeof(wchar_t);

std::wstring_view device_path_view(device_path_str);
base::FilePath device_path(device_path_str);
*out_path = base::FilePath(
    drive + nt_device_path.value().substr(device_path_view.length()));
```

Good:

```cpp
// Good: Query the source string or existing FilePath directly.
const size_t size_with_null = (target_str.size() + 1) * sizeof(wchar_t);

base::FilePath device_path(device_path_str);
*out_path = base::FilePath(
    drive + nt_device_path.value().substr(device_path.value().size()));
```
