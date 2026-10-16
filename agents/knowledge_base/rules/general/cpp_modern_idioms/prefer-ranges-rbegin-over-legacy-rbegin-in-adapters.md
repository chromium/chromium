---
name: general/prefer-ranges-rbegin-over-legacy-rbegin-in-adapters
rubric: readability
severity: nit
---

**Rule.** In generic range adapters and reverse-traversal utilities, prefer
'std::ranges::rbegin' and 'std::ranges::rend' over legacy 'std::rbegin' and
'std::rend'. This allows reverse iteration over all bidirectional ranges without
requiring them to define explicit reverse-iterator member or ADL functions.

**Why.** Unlike legacy 'std::rbegin' which requires explicit 'rbegin()' member
or ADL overloads, C++20 customization point objects 'std::ranges::rbegin' and
'std::ranges::rend' automatically fall back to constructing
'std::reverse_iterator' from 'ranges::end()' and 'ranges::begin()'. This avoids
unnecessary boilerplate reverse-iterator definitions on adapted range types.

## Examples

Bad:

```cpp
template <typename Range>
class ReversedAdapter {
 public:
  // Fails on bidirectional ranges that lack explicit rbegin()/rend() members
  auto begin() { return std::rbegin(range_); }
  auto end() { return std::rend(range_); }
};
```

Good:

```cpp
template <typename Range>
class ReversedAdapter {
 public:
  // std::ranges::rbegin synthesizes reverse iterators from begin/end automatically
  auto begin() { return std::ranges::rbegin(range_); }
  auto end() { return std::ranges::rend(range_); }
};
```
