---
name: general/group-related-member-variables-in-structs-and-classes
rubric: readability
severity: nit
---

**Rule.** Group logically related data members together within struct or class
definitions (for example, placing feature-specific options, indices, or buffers
adjacent to their corresponding enable flags) rather than appending new fields
to the bottom of the declaration.

**Why.** Colocating related members keeps configuration state and feature
parameters discoverable, documents which fields belong together, and avoids
forcing readers to search through unrelated fields.

## Examples

Bad:

```cpp
struct Settings {
  bool with_thread_cache = false;
  // ... unrelated settings ...
  bool enable_free_with_size = false;
  size_t thread_cache_index = 0;
};
```

Good:

```cpp
struct Settings {
  bool with_thread_cache = false;
  size_t thread_cache_index = 0;
  // ... unrelated settings ...
  bool enable_free_with_size = false;
};
```
