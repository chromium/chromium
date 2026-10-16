---
name: general/avoid-utf16-to-utf8-for-ascii-parsing
rubric: safety
severity: should_fix
---

**Rule.** Do not convert 16-bit string views (`std::u16string_view` or
`std::wstring_view`) to `std::string` using `base::UTF16ToUTF8()` or
`base::WideToUTF8()` solely to reuse an 8-bit parsing or validation function
when processing ASCII-structured data (such as UUIDs, hexadecimal strings,
numeric tokens, or delimiters). Instead, template the parser on the character or
string view type, or provide overloads that inspect character code units
directly.

**Why.** Converting a 16-bit string view to `std::string` via
`base::UTF16ToUTF8()` performs an unnecessary heap allocation and a full
character transcoding pass. Because 7-bit ASCII characters map identically into
16-bit code units, structured ASCII formats can be validated and parsed
character-by-character directly from string views with zero allocations.

## Examples

Bad:

```cpp
// static
Uuid Uuid::ParseCaseInsensitive(std::u16string_view input) {
  // Unnecessary heap allocation and UTF-16 to UTF-8 transcoding.
  return Parse(base::UTF16ToUTF8(input), /*strict=*/false);
}
```

Good:

```cpp
// Template the parser to accept both std::string_view and std::u16string_view.
template <typename Char>
constexpr bool IsLowerHexDigit(Char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

template <typename StringViewType>
Uuid Uuid::Parse(StringViewType input, bool strict) {
  // Directly inspect characters without transcoding or heap allocation.
  ...
}

// static
Uuid Uuid::ParseCaseInsensitive(std::u16string_view input) {
  return Parse(input, /*strict=*/false);
}
```
