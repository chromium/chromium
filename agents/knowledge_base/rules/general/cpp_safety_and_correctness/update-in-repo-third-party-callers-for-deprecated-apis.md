---
name: general/update-in-repo-third-party-callers-for-deprecated-apis
rubric: correctness
severity: should_fix
---

**Rule.** Do not retain obsolete or deprecated base APIs solely to accommodate
callers in in-tree third-party dependencies. When all in-Chromium callers have
migrated, update vendored third-party callers in the same CL, regenerate
corresponding patch files (e.g., patches/\* or google.patch), and obtain the
third_party directory OWNERS' approval to delete the deprecated API.

**Why.** Vendored third-party libraries live directly in the Chromium repository
and can be modified with local patches. Retaining obsolete functions or
raw-pointer overloads solely for in-repo third-party callers creates unnecessary
technical debt and delays cleanup across the codebase.

## Examples

Bad:

```cpp
// In base/strings/utf_string_conversion_utils.h:
// Retaining deprecated pointer overload solely for a third_party/zxcvbn-cpp caller:
UNSAFE_BUFFER_USAGE BASE_EXPORT bool ReadUnicodeCharacter(
    const char* src, size_t src_len, size_t* char_index,
    base_icu::UChar32* code_point_out);
```

Good:

```cpp
// In third_party/zxcvbn-cpp/native-src/zxcvbn/util.cpp:
base::ReadUnicodeCharacter(std::string_view(it, end), &char_index,
                           &code_point_out);

// Regenerate third_party/zxcvbn-cpp/google.patch and delete the
// deprecated ReadUnicodeCharacter overload from base/.
```
