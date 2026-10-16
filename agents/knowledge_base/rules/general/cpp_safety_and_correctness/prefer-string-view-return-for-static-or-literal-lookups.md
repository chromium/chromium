---
name: general/prefer-string-view-return-for-static-or-literal-lookups
rubric: safety
severity: should_fix
---

**Rule.** Prefer returning std::string_view (or std::u16string_view /
std::wstring_view) instead of an owning std::string or std::wstring when
returning string literals, static constants, strings backed by long-lived lookup
structures (such as static enum option tables), or views over existing in-memory
string buffers and OS structs (such as Windows UNICODE_STRING). Reserve
returning owning string types for functions that dynamically construct,
allocate, or transfer ownership of a new string.

**Why.** Returning an owning std::string or std::wstring for string literals,
lookup table entries, or pre-existing in-memory buffers incurs unnecessary heap
allocations and character buffer copying. Returning a string view is zero-cost
and avoids heap overhead whenever the underlying character data outlives the
returned view.

## Examples

Bad:

```cpp
// Bad: Allocates an owned std::string for static lookup entries.
template <typename Enum>
std::string GetFieldTrialEnumAsString(
    base::span<const Option> options,
    Enum enum_value) {
  for (const auto& option : options) {
    if (enum_value == option.value) {
      return option.name;
    }
  }
  return "<unknown value>";
}

// Bad: Allocates an owned std::wstring when converting a read-only buffer.
inline std::optional<std::wstring> UnicodeStringToWString(
    const UNICODE_STRING& us) {
  if (!us.Buffer || !us.Length) {
    return std::nullopt;
  }
  const size_t length = us.Length / sizeof(wchar_t);
  return std::wstring(us.Buffer, us.Buffer + length);
}
```

Good:

```cpp
// Good: Returns std::string_view without allocating temporary strings.
template <typename Enum>
std::string_view GetFieldTrialEnumAsStringView(
    base::span<const Option> options,
    Enum enum_value) {
  for (const auto& option : options) {
    if (enum_value == option.value) {
      return option.name;
    }
  }
  return "<unknown value>";
}

// Good: Returns std::wstring_view over the existing buffer with zero copies.
inline std::wstring_view UnicodeStringToView(const UNICODE_STRING& us) {
  return std::wstring_view(us.Buffer, us.Length / sizeof(wchar_t));
}
```
