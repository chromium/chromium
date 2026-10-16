---
name: general/encapsulate-object-construction-and-parsing
rubric: design
severity: should_fix
---

**Rule.** Encapsulate complex object construction, parsing, and multi-field
initialization inside a constructor or static factory method (such as Create(),
CreateFrom...(), or Parse()) on the type itself. Do not have external callers or
free functions construct an empty or uninitialized instance and populate its
internal fields piecemeal.

**Why.** Types should maintain and encapsulate their own construction and
parsing invariants. Placing construction and parsing logic within the type keeps
objects self-contained, protects cross-field invariants, and prevents callers
from depending on or manipulating internal field layout.

## Examples

Bad:

```cpp
ParsedConfig ParseConfig(std::string_view raw) {
  ParsedConfig config;
  config.header = ExtractHeader(raw);
  config.payload = ExtractPayload(raw);
  config.is_valid = Validate(config.header, config.payload);
  return config;
}
```

Good:

```cpp
// static
std::optional<ParsedConfig> ParsedConfig::CreateFromRawString(
    std::string_view raw) {
  ParsedConfig config;
  config.header = ExtractHeader(raw);
  config.payload = ExtractPayload(raw);
  if (!Validate(config.header, config.payload)) {
    return std::nullopt;
  }
  return config;
}

std::optional<ParsedConfig> GetConfig(std::string_view raw) {
  return ParsedConfig::CreateFromRawString(raw);
}
```
