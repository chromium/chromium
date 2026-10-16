---
name: general/prefer-base-strcat-over-chained-plus-operator
rubric: readability
severity: should_fix
---

**Rule.** Prefer base::StrCat() for concatenating strings, string views, or
literals, and base::StrAppend() for appending onto an existing string, rather
than chaining operator+ or calling std::string::append. Use base::StrCat to
compute total needed size upfront and allocate once.

**Why.** Chaining operator+ creates temporary string instances and causes
multiple heap allocations and buffer copies. base::StrCat and base::StrAppend
calculate total required length upfront, avoiding redundant reallocations and
keeping string concatenation idiomatic and consistent across the codebase.

## Examples

Bad:

```cpp
// Chained operator+
std::string path = dir + "/" + filename + "." + ext;

// Calling append when constructing a concatenated string
std::wstring full = name.append(L"://ISOLATION");
```

Good:

```cpp
// Concatenating multiple parts
std::string path = base::StrCat({dir, "/", filename, ".", ext});

// Concatenating or appending with base::StrCat / base::StrAppend
std::wstring full = base::StrCat({name, L"://ISOLATION"});
```
