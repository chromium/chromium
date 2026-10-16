---
name: general/prefer-ranges-for-each-over-transform-for-in-place-modificat
rubric: readability
severity: nit
---

**Rule.** When mutating or modifying elements of a container or range in-place,
prefer std::ranges::for_each (or a range-based for loop) with a lambda accepting
elements by non-const reference over std::ranges::transform passing output
destination iterators.

**Why.** std::ranges::transform models mapping inputs to a separate destination
range and requires passing explicit iterator pairs when modifying elements in
place. std::ranges::for_each accepts the range directly and cleanly expresses
in-place element transformation without iterator boilerplate.

## Examples

Bad:

```cpp
std::ranges::transform(name.begin(), name.end(), name.begin(),
                       [](wchar_t c) { return towupper(c); });
```

Good:

```cpp
std::ranges::for_each(name, [](wchar_t& c) { c = towupper(c); });
```
