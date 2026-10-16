---
name: general/prefer-try-emplace-to-avoid-redundant-lookups
rubric: safety
severity: optional
---

**Rule.** When inserting an element into an associative container (such as
std::map, base::flat_map, or absl::flat_hash_map) only if the key is absent, use
try_emplace() (or emplace() / insert()) rather than performing a separate find()
or contains() lookup followed by operator[] or insertion. Unpack the returned
iterator and boolean flag to handle existing vs. newly inserted elements in a
single container lookup.

**Why.** Checking whether a key exists using find() or contains() and then
indexing via operator[] or calling insert() executes two distinct container
traversals or hash lookups. Using try_emplace() performs a single lookup,
constructing and inserting the value only when the key is absent while returning
an iterator to the entry regardless of whether insertion occurred.

## Examples

Bad:

```cpp
// Redundant find() lookup followed by operator[] insertion.
auto it = feature_map_.find(feature.name);
if (it == feature_map_.end()) {
  feature_map_[feature.name] = &feature;
  return true;
}
return it->second == &feature;
```

Good:

```cpp
// Single lookup handles insertion if absent and returns entry iterator.
auto [it, inserted] =
    feature_map_.try_emplace(feature.name, &feature);
return inserted ? true : it->second == &feature;
```
