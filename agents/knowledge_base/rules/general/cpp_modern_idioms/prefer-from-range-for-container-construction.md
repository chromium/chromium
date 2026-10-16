---
name: general/prefer-from-range-for-container-construction
rubric: readability
severity: should_fix
---

**Rule.** When constructing standard containers (such as std::vector or
std::set) or std::string from a compatible range, span, or container, use the
C++23 std::from_range constructor tag. Avoid passing begin/end iterator pairs or
wrapping ranges in intermediate view helpers (such as base::as_string_view)
solely to construct owning containers.

**Why.** The C++23 std::from_range constructor provides a standard, direct, and
concise way to initialize owning containers and strings from compatible ranges
or spans. It eliminates iterator boilerplate, avoids redundant intermediate view
adapters, and removes unnecessary header dependencies.

## Examples

Bad:

```cpp
// Bad: passing iterator pairs or wrapping spans in intermediate views
std::vector<int> copy(items.begin(), items.end());
const std::string pattern(base::as_string_view(pattern_span));
```

Good:

```cpp
// Good: direct range construction using std::from_range
std::vector<int> copy(std::from_range, items);
const std::string pattern(std::from_range, pattern_span);
```
