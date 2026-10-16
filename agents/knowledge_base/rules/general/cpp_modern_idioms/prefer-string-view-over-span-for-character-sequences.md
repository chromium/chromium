---
name: general/prefer-string-view-over-span-for-character-sequences
rubric: readability
severity: should_fix
---

**Rule.** Prefer std::string_view (or std::basic_string_view<CharT> for
templated character types, std::u16string_view, std::wstring_view) over
base::span<const CharT> when accepting read-only string or character-sequence
inputs. Reserve base::span for arbitrary contiguous byte/memory buffers or
non-textual element arrays.

**Why.** std::string_view communicates string semantics, provides rich text
manipulation utilities (such as find, substr, and prefix/suffix checks), and
interoperates seamlessly with std::string and string literals without requiring
explicit conversion via base::as_string_view.

## Examples

Bad:

```cpp
template <typename CharT>
void PrepareForUTF8Output(base::span<const CharT> src, std::string* output);

void ProcessHeader(base::span<const char> header_line);
```

Good:

```cpp
template <typename CharT>
void PrepareForUTF8Output(std::basic_string_view<CharT> src,
                          std::string* output);

void ProcessHeader(std::string_view header_line);
```
