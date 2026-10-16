---
name: general/prefer-standard-string-view-aliases-over-basic-string-view
rubric: readability
severity: should_fix
---

**Rule.** Prefer standard string view type aliases (std::string_view,
std::u16string_view, std::u32string_view, std::wstring_view) over spelling out
std::basic_string_view<CharT> when referring to concrete character types (such
as char, char16_t, char32_t, or wchar_t). Reserve std::basic_string_view<CharT>
strictly for generic templates parameterized on the character type.

**Why.** Standard C++ type aliases are more idiomatic, concise, and consistent
across the codebase. Spelling out std::basic_string_view<CharT> for concrete
character types adds visual noise and is only necessary in templated code where
the character type is a template parameter.

## Examples

Bad:

```cpp
template void PrepareForUTF8Output(std::basic_string_view<char16_t> str,
                                   std::string* output);
void ProcessPath(std::basic_string_view<char> path);
```

Good:

```cpp
template void PrepareForUTF8Output(std::u16string_view str,
                                   std::string* output);
void ProcessPath(std::string_view path);
```
