---
name: general/prefer-cstring-view-for-null-terminated-c-api-strings
rubric: safety
severity: should_fix
---

**Rule.** Use `base::cstring_view` (or `base::wcstring_view`,
`base::u16cstring_view`, or platform aliases like
`NativeEnvironmentCStringView`) by value instead of `const char*`,
`const wchar_t*`, or `std::string_view` for string parameters that require NUL
termination (such as passing to C/OS APIs or parsing NUL-delimited blocks) or
that are populated from string literals to initialize string view members. When
obtaining the underlying NUL-terminated pointer to pass to a C-style API, call
`.c_str()` rather than `.data()`.

**Why.** `std::string_view` represents an arbitrary string slice and is not
guaranteed to be NUL-terminated, risking buffer overruns if passed to C APIs.
Passing string literals to raw `const char*` parameters discards compile-time
array lengths, forcing runtime `strlen` invocations when initializing
`std::string_view` members. `base::cstring_view` preserves compile-time literal
lengths via its array constructor, guarantees NUL termination in the type
system, and provides `.c_str()` for safe C-API interop.

## Examples

Bad:

```cpp
// Bad: std::string_view is not NUL-terminated; passing .data() to C APIs risks buffer overruns.
FILE* OpenFile(const FilePath& filename, std::string_view mode) {
  return fopen(filename.value().c_str(), mode.data());
}

// Bad: const char* loses literal length, causing runtime strlen when initializing string_view.
constexpr Feature(const char* name,
                  FeatureState default_state)
    : name(name), default_state(default_state) {}

// Bad: Calling .data() on cstring_view instead of .c_str() when passing to C APIs.
InitPropVariantFromString(property_string_value.data(),
                          property_value.Receive());
```

Good:

```cpp
// Good: base::cstring_view guarantees NUL termination via .c_str().
FILE* OpenFile(const FilePath& filename, base::cstring_view mode) {
  return fopen(filename.value().c_str(), mode.c_str());
}

// Good: base::cstring_view preserves compile-time literal length without strlen().
constexpr Feature(base::cstring_view name,
                  FeatureState default_state)
    : name(name), default_state(default_state) {}

// Good: Call .c_str() to obtain a NUL-terminated string pointer for C APIs.
InitPropVariantFromString(property_string_value.c_str(),
                          property_value.Receive());
```
