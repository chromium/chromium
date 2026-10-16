---
name: general/consolidate-duplicate-lookup-helpers
rubric: design
severity: should_fix
---

**Rule.** Do not duplicate search, lookup, or iteration logic across member
methods and free utility functions. Extract and consolidate the common iteration
logic into a shared helper function and delegate to it from both member and free
callers.

**Why.** Duplicating lookup and iteration loops across member and free functions
introduces maintenance overhead, risks behavioral drift, and for templated
functions can lead to redundant template instantiations that increase binary
size.

## Examples

Bad:

```cpp
// FeatureParam<Enum, true>::GetName() already iterates over options:
std::string GetName(Enum value) const {
  for (const auto& option : options) {
    if (value == option.value) return option.name;
  }
  NOTREACHED();
}

// Anti-pattern: Adding a free function that duplicates the lookup loop:
template <typename Enum>
std::string GetFieldTrialEnumAsString(
    base::span<const typename FeatureParam<Enum, true>::Option> options,
    Enum enum_value) {
  for (const auto& option : options) {
    if (enum_value == option.value) return option.name;
  }
  return "<unknown value>";
}
```

Good:

```cpp
// Consolidate into a shared helper function:
template <typename Enum>
std::string_view GetFieldTrialEnumAsStringView(
    base::span<const typename FeatureParam<Enum, true>::Option> options,
    Enum value) {
  for (const auto& option : options) {
    if (value == option.value) return option.name;
  }
  return "<unknown value>";
}

std::string GetName(Enum value) const {
  return std::string{GetFieldTrialEnumAsStringView(options, value)};
}
```
