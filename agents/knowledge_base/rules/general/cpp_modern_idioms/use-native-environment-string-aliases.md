---
name: general/use-native-environment-string-aliases
rubric: readability
severity: should_fix
---

**Rule.** Use canonical NativeEnvironmentString, NativeEnvironmentStringView, or
NativeEnvironmentCStringView type aliases rather than concrete string types like
std::wstring or std::string when storing, parsing, or passing platform-native
environment variables and blocks.

**Why.** Environment string representations differ between platforms
(std::wstring on Windows, std::string on POSIX and Fuchsia). Using the canonical
type aliases ensures portability, communicates semantic intent, and maintains
consistency with base::EnvironmentMap and base environment APIs.

## Examples

Bad:

```cpp
// Storing concrete string types for environment data:
std::wstring unparsed_;
std::wstring_view key;
```

Good:

```cpp
// Using canonical platform-abstracted aliases:
NativeEnvironmentString unparsed_;
NativeEnvironmentStringView key;
```
