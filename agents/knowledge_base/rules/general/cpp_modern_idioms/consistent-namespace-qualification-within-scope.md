---
name: general/consistent-namespace-qualification-within-scope
rubric: readability
severity: should_fix
---

**Rule.** Do not qualify symbols with their enclosing or parent namespace (e.g.,
using `base::` while inside `namespace base` or `namespace base::win`) unless
explicitly required by the compiler to disambiguate a name collision. Maintain
consistent qualification style throughout the file or function rather than
arbitrarily mixing qualified and unqualified references to enclosing symbols.

**Why.** In C++, symbols in enclosing namespaces are directly accessible via
unqualified lookup; redundant qualifiers add visual clutter without providing
value. Mixing qualified and unqualified references to the same enclosing
namespace creates visual inconsistency and harms readability.

## Examples

Bad:

```cpp
// Inside namespace base::win:
const std::wstring settings_url = base::StrCat(
    {L"ms-settings:defaultapps?",
     base::ASCIIToWide(base::EscapeQueryParamValue(base::WideToUTF8(app_name),
                                                  /*use_plus=*/false))});

// Inside namespace base::internal (mixing qualified and unqualified):
base::HeapArray<char*> AlterEnvironment(const base::span<const char* const> env) {
  size_t len = ParseEnvLine(UNSAFE_BUFFERS(cstring_view(env_var)), &key);
}
```

Good:

```cpp
// Inside namespace base::win:
const std::wstring settings_url = StrCat(
    {L"ms-settings:defaultapps?",
     ASCIIToWide(EscapeQueryParamValue(WideToUTF8(app_name),
                                       /*use_plus=*/false))});

// Inside namespace base::internal (consistent unqualified usage):
HeapArray<char*> AlterEnvironment(const span<const char* const> env) {
  size_t len = ParseEnvLine(UNSAFE_BUFFERS(cstring_view(env_var)), &key);
}
```
