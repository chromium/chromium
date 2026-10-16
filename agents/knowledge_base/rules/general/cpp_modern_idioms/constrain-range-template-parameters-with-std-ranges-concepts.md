---
name: general/constrain-range-template-parameters-with-std-ranges-concepts
rubric: readability
severity: should_fix
---

**Rule.** When declaring template functions, constructors, or container methods
that accept generic range arguments, constrain the template parameter with
requires(std::ranges::input_range<Range>) (or the appropriate standard range
concept such as std::ranges::forward_range or std::ranges::range). Avoid leaving
range template parameters unconstrained.

**Why.** Unconstrained template parameters participate in overload resolution
for arbitrary types, deferring type mismatch errors deep into the template body
with verbose compiler diagnostics. Constraining templates with standard range
concepts provides early, clear diagnostics at the call site and prevents
unintended overload resolution matches.

## Examples

Bad:

```cpp
template <class Range>
flat_tree(std::from_range_t,
          sorted_unique_t,
          const Range& range,
          const key_compare& comp = key_compare());
```

Good:

```cpp
template <class Range>
  requires(std::ranges::input_range<Range>)
flat_tree(std::from_range_t,
          sorted_unique_t,
          const Range& range,
          const key_compare& comp = key_compare());
```
