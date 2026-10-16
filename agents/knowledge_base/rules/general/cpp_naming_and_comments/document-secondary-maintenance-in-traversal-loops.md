---
name: general/document-secondary-maintenance-in-traversal-loops
rubric: readability
severity: nit
---

**Rule.** When a traversal or search loop performs secondary maintenance work or
state accumulation while iterating (such as incrementally rebuilding a Bloom
filter, cache, or auxiliary index before reaching the target), add an inline
comment explaining that secondary purpose. Clarify what state is being updated
on the fly so readers understand why the loop is not solely performing a lookup.

**Why.** Traversing loops are typically expected to only search for a target
element. When a search loop combines element lookup with piggybacked maintenance
or state accumulation, documenting the secondary purpose inline prevents
maintainers from mistaking the state update for dead code or an accidental side
effect.

## Examples

Bad:

```cpp
for (; node != nullptr; node = node->next) {
  void* node_key = node->key.load(std::memory_order_relaxed);
  if (node_key == key) {
    break;
  }
  if (bloom_filters_enabled_ && node_key != nullptr) {
    bits |= bucket.filter.GetBitsForKey(node_key);
  }
}
```

Good:

```cpp
for (; node != nullptr; node = node->next) {
  void* node_key = node->key.load(std::memory_order_relaxed);
  if (node_key == key) {
    break;
  }
  // Rebuild the bloom filter as we go, without the removed key.
  if (bloom_filters_enabled_ && node_key != nullptr) {
    bits |= bucket.filter.GetBitsForKey(node_key);
  }
}
```
