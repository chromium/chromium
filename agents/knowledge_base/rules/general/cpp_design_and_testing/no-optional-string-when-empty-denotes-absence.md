---
name: general/no-optional-string-when-empty-denotes-absence
rubric: design
severity: should_fix
---

**Rule.** Avoid wrapping return values or callback results in
std::optional<std::string> or std::optional<std::string_view> when an empty
string already represents absence, omission, or an invalid lookup. Return
std::string or std::string_view directly and document or rely on .empty() to
indicate the absence of a value.

**Why.** When an empty string has no distinct valid meaning separate from
absence (such as in name mappers, identifier lookups, or sanitizers),
std::optional introduces redundant disengaged states (both std::nullopt and "").
This adds unnecessary wrapper boilerplate and causes bugs when callers check
!opt.has_value() but forget to check opt->empty().

## Examples

Bad:

```cpp
using MapperCallback =
    base::RepeatingCallback<std::optional<std::string_view>(std::string_view)>;

if (mapper) {
  auto mapped_name = mapper.Run(histogram_name);
  if (!mapped_name) {
    return DummyHistogram::GetInstance();
  }
  name_to_use = *mapped_name;
}
```

Good:

```cpp
using MapperCallback =
    base::RepeatingCallback<std::string_view(std::string_view)>;

if (mapper) {
  name_to_use = mapper.Run(histogram_name);
  if (name_to_use.empty()) {
    return DummyHistogram::GetInstance();
  }
}
```
