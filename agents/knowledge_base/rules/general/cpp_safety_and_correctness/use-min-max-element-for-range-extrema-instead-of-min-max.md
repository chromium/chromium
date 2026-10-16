---
name: general/use-min-max-element-for-range-extrema-instead-of-min-max
rubric: correctness
severity: nit
---

**Rule.** Do not pass range iterators (such as begin() and end()) to std::min or
std::max to find the extremum of a range. Use std::min_element,
std::max_element, std::ranges::min_element, std::ranges::max_element, or
std::ranges::min/max instead.

**Why.** std::min(a, b) and std::max(a, b) compare their two arguments directly
rather than dereferencing them. When passed iterator pairs where begin < end,
std::min(begin, end) compares iterator positions and always evaluates to begin,
quietly returning the first element instead of finding the range's actual
extremum.

## Examples

Bad:

```cpp
// Bug: Compares iterator positions rather than dereferenced values, returning frequencies.begin().
uint64_t min_frequency = *std::min(frequencies.begin(), frequencies.end());
```

Good:

```cpp
// Correct: Finds the iterator pointing to the minimum element in the range.
uint64_t min_frequency = *std::min_element(frequencies.begin(), frequencies.end());
// Or using C++20 ranges:
uint64_t min_frequency_ranges = std::ranges::min(frequencies);
```
