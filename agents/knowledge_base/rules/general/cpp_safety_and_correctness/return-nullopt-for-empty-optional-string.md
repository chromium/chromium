---
name: general/return-nullopt-for-empty-optional-string
rubric: correctness
severity: should_fix
---

**Rule.** When implementing a function that returns an std::optional string,
string view, or container to represent an extracted component or token, verify
that the extracted value is not empty (e.g., via .empty()) and return
std::nullopt when absent or empty. Do not return an empty string directly, as
implicit conversion creates an engaged std::optional (has_value() == true)
containing an empty value.

**Why.** Implicit conversion from std::string, std::string_view, or
FilePath::StringType to std::optional creates an engaged optional rather than
std::nullopt. Callers checking if (result) or if (result.has_value()) will treat
the empty string as a present value, violating API contracts that signal absence
or extraction failure via std::nullopt.

## Examples

Bad:

```cpp
std::optional<std::string> ExtractToken(std::string_view input, size_t pos) {
  const std::string token = std::string(input.substr(0, pos));
  // BUG: If pos == 0, returns an engaged optional containing an empty string ("").
  return token;
}
```

Good:

```cpp
std::optional<std::string> ExtractToken(std::string_view input, size_t pos) {
  const std::string token = std::string(input.substr(0, pos));
  if (token.empty()) {
    return std::nullopt;
  }
  return token;
}
```
