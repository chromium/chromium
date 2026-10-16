---
name: general/prefer-member-contains-over-ranges-contains-on-associative-c
rubric: safety
severity: nit
---

**Rule.** Do not use std::ranges::contains(), std::ranges::find(), or other
linear range search algorithms on associative, flat, or hash containers (such as
std::set, std::map, base::flat_set, base::flat_map, absl::flat_hash_set, or
absl::flat_hash_map). Use the container's member .contains() or .find() methods
instead.

**Why.** std::ranges::contains and std::ranges::find iterate sequentially from
begin() to end(), performing an O(N) linear scan that silently bypasses the
O(log N) or O(1) lookup structure of associative and hash containers.

## Examples

Bad:

```cpp
// Performs an O(N) linear search across all elements:
if (std::ranges::contains(my_set, key)) {
  Process(key);
}
```

Good:

```cpp
// Performs an O(log N) or O(1) container lookup:
if (my_set.contains(key)) {
  Process(key);
}
```
