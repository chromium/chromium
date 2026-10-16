---
name: general/group-related-function-overloads-together
rubric: readability
severity: should_fix
---

**Rule.** When declaring overloaded functions or constructors—such as modern
replacements (like base::span or std::string_view) alongside legacy
pointer-and-size APIs, or unchecked variants (like unchecked_t) alongside
checked overloads—place corresponding overloads directly adjacent to each other
rather than separating them into disjoint blocks. Maintain a consistent
declaration order across overload groups, and prefer listing the standard, safe,
or checked overload before unchecked, opt-out, or legacy variants.

**Why.** Colocating related overloads makes safe alternatives immediately
discoverable alongside legacy or opt-out counterparts and avoids duplicating
conditional compilation #if blocks. Consistent ordering with standard/checked
variants declared first highlights the preferred default API for callers and
keeps opt-out variants secondary.

## Examples

Bad:

```cpp
// Modern view overloads separated from legacy pointer-and-size counterparts:
BASE_EXPORT bool ReadUnicodeCharacter(std::string_view src, ...);
BASE_EXPORT bool ReadUnicodeCharacter(std::u16string_view src, ...);
#if defined(WCHAR_T_IS_32_BIT)
BASE_EXPORT bool ReadUnicodeCharacter(std::wstring_view src, ...);
#endif

// Legacy overloads placed in a separate block with duplicate #if guards:
BASE_EXPORT bool ReadUnicodeCharacter(const char* src, size_t src_len, ...);
BASE_EXPORT bool ReadUnicodeCharacter(const char16_t* src, size_t src_len, ...);
#if defined(WCHAR_T_IS_32_BIT)
BASE_EXPORT bool ReadUnicodeCharacter(const wchar_t* src, size_t src_len, ...);
#endif

// Inconsistent ordering between checked and unchecked overloads:
constexpr span(unchecked_t tag, It first, StrictNumeric<size_type> count);
constexpr span(It first, StrictNumeric<size_type> count);

constexpr span(It first, End last);
constexpr span(unchecked_t tag, It first, End last);
```

Good:

```cpp
// Group corresponding overloads together, placing safe/view overloads first:
BASE_EXPORT bool ReadUnicodeCharacter(std::string_view src, ...);
BASE_EXPORT bool ReadUnicodeCharacter(const char* src, size_t src_len, ...);

BASE_EXPORT bool ReadUnicodeCharacter(std::u16string_view src, ...);
BASE_EXPORT bool ReadUnicodeCharacter(const char16_t* src, size_t src_len, ...);

#if defined(WCHAR_T_IS_32_BIT)
BASE_EXPORT bool ReadUnicodeCharacter(std::wstring_view src, ...);
BASE_EXPORT bool ReadUnicodeCharacter(const wchar_t* src, size_t src_len, ...);
#endif

// Consistent ordering: standard/checked overloads precede unchecked variants:
constexpr span(It first, StrictNumeric<size_type> count);
constexpr span(unchecked_t tag, It first, StrictNumeric<size_type> count);

constexpr span(It first, End last);
constexpr span(unchecked_t tag, It first, End last);
```
