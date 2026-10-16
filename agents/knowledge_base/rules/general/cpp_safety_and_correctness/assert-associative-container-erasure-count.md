---
name: general/assert-associative-container-erasure-count
rubric: safety
severity: nit
---

**Rule.** When erasing an element by key from an associative or hash container
(such as std::set, std::map, base::flat_set, base::flat_map,
absl::flat_hash_set, or absl::flat_hash_map) and asserting that the key was
present, call container.erase(key) directly and assert that the returned count
is 1 (e.g., via CHECK_EQ(container.erase(key), 1u) or
CHECK(container.erase(key))). Do not perform a separate find() lookup to assert
presence before calling erase(iterator).

**Why.** In associative and hash containers, erase(key) performs the lookup and
removal in a single operation and returns the number of elements removed.
Performing a prior find() lookup merely to assert presence introduces redundant
lookup overhead and unnecessary iterator boilerplate.

## Examples

Bad:

```cpp
auto it = sources_.find(source);
CHECK(it != sources_.end());
sources_.erase(it);
```

Good:

```cpp
CHECK_EQ(sources_.erase(source), 1u);
```
