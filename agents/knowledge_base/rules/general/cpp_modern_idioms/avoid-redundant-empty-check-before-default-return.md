---
name: general/avoid-redundant-empty-check-before-default-return
rubric: readability
severity: nit
---

**Rule.** Do not add redundant emptiness or default-value checks (such as
`|| value.empty()`) to error-handling guard conditions when the error branch
returns a default-constructed instance and the success path already returns that
variable. Rely on the success return statement to propagate the empty or default
result directly.

**Why.** When an error condition returns a default-constructed (empty) instance,
checking whether the output variable is empty in the error guard is superfluous
if the success path returning that variable already yields the identical empty
object. Omitting the redundant check simplifies error conditions and avoids
unnecessary branching.

## Examples

Bad:

```cpp
std::wstring GetMachineGuid() {
  base::win::RegKey key;
  std::wstring value;
  if (key.Open(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography",
               KEY_QUERY_VALUE | KEY_WOW64_64KEY) != ERROR_SUCCESS ||
      key.ReadValue(L"MachineGuid", &value) != ERROR_SUCCESS || value.empty()) {
    return std::wstring();
  }
  return value;
}
```

Good:

```cpp
std::wstring GetMachineGuid() {
  base::win::RegKey key;
  std::wstring value;
  if (key.Open(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography",
               KEY_QUERY_VALUE | KEY_WOW64_64KEY) != ERROR_SUCCESS ||
      key.ReadValue(L"MachineGuid", &value) != ERROR_SUCCESS) {
    return std::wstring();
  }
  return value;
}
```
