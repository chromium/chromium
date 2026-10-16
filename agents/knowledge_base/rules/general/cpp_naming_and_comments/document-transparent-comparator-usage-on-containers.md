---
name: general/document-transparent-comparator-usage-on-containers
rubric: readability
severity: optional
---

**Rule.** When specifying std::less\<> (or another transparent comparator) on
associative container type aliases or declarations (such as std::map or
std::set), add a clarifying comment explaining that it is used to enable
transparent (heterogeneous) lookup. Document which view types (such as
std::string_view or base::cstring_view) can be searched without allocating
temporary objects.

**Why.** Ordered standard library containers default to std::less<Key>, so
explicitly specifying std::less\<> can appear redundant or confusing to
maintainers unfamiliar with heterogeneous lookup. A brief comment documents why
the non-default comparator was supplied and informs callers that lightweight
view types can be passed to lookup methods without temporary heap allocations.

## Examples

Bad:

```cpp
// Map of environment variable names to values.
using EnvironmentMap =
    std::map<NativeEnvironmentString, NativeEnvironmentString, std::less<>>;
```

Good:

```cpp
// EnvironmentMap uses std::less<> to enable transparent (heterogeneous) lookup,
// allowing NativeEnvironmentCStringView to be used for search without creating
// temporary std::string or std::wstring objects.
using EnvironmentMap =
    std::map<NativeEnvironmentString, NativeEnvironmentString, std::less<>>;
```
