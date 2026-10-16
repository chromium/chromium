---
name: general/prefer-empty-braces-over-zero-initializer
rubric: readability
severity: nit
---

**Rule.** Prefer empty braces '{}' (e.g. '= {}') rather than '{0}' to
zero-initialize aggregates, std::array, structs, or fixed-size C arrays.

**Why.** In C++, '{0}' is an unnecessary legacy C idiom that explicitly
initializes only the first element to zero while copy-initializing remaining
elements. Using '{}' directly and uniformly expresses value-initialization
across all elements.

## Examples

Bad:

```cpp
std::array<uint32_t, kMaxCount> leases = {0};
MyAggregate data = {0};
int buffer[64] = {0};
```

Good:

```cpp
std::array<uint32_t, kMaxCount> leases = {};
MyAggregate data = {};
int buffer[64] = {};
```
