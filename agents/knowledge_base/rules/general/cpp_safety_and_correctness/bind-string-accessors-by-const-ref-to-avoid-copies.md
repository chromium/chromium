---
name: general/bind-string-accessors-by-const-ref-to-avoid-copies
rubric: safety
severity: nit
---

**Rule.** When assigning or binding the result of an accessor or getter that
returns const std::string& (or a container by const reference) to a local
variable, bind it by const std::string&, std::string_view, or const auto& rather
than copying into an owning local variable, unless an independent or mutated
copy is required.

**Why.** Assigning the return value of a reference-returning accessor to a local
std::string invokes the copy constructor, triggering unnecessary heap
allocations and character buffer copies. Binding by const reference or string
view provides zero-cost access to the existing string buffer.

## Examples

Bad:

```cpp
std::string host_package_name;
#if BUILDFLAG(IS_ANDROID)
  host_package_name = android::apk_info::host_package_name();
#endif
```

Good:

```cpp
#if BUILDFLAG(IS_ANDROID)
  const std::string& host_package_name = android::apk_info::host_package_name();
#else
  std::string host_package_name;
#endif
```
