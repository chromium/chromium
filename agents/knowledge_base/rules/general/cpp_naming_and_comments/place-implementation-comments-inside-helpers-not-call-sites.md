---
name: general/place-implementation-comments-inside-helpers-not-call-sites
rubric: readability
severity: nit
---

**Rule.** Place implementation comments explaining internal mechanics,
concurrency invariants, or algorithmic details inside the helper function where
the logic executes, rather than at external call sites. Additionally, inside
standalone or helper functions, keep comments self-contained and explain
rationale using local constructs and APIs rather than coupling comments to
specific caller objects, caches, or external call-site contexts.

**Why.** Documenting callee implementation choices at call sites clutters caller
functions, hides essential rationale from maintainers modifying the helper
directly, and causes comments to drift out of sync. Conversely, referencing
caller-specific classes or caches inside standalone helpers couples the helper
to one caller and makes comments misleading when read in isolation or reused
across multiple call sites.

## Examples

Bad:

```cpp
// Variant 1: Documenting helper implementation mechanics at the call site.
// We need to use a loop to preserve the logging bits (bits 16 and 17),
// which might be set concurrently by RegisterFeatureAccess().
SetFeatureCachedBits(feature.cached_value, new_cache_value);

// Variant 2: Referencing caller-specific context/cache inside a helper.
bool IsLink(const FilePath& file_path) {
  // Query attributes directly; cached attributes in FileEnumerator's
  // find_data() may be stale and lack FILE_ATTRIBUTE_REPARSE_POINT.
  return (::GetFileAttributes(file_path.value().c_str()) &
          FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}
```

Good:

```cpp
// Variant 1: Placing implementation rationale inside the helper function.
void SetFeatureCachedBits(std::atomic<Feature::FeatureStateCache>& cached_value,
                          Feature::FeatureStateCache new_value) {
  Feature::FeatureStateCache expected =
      cached_value.load(std::memory_order_relaxed);
  Feature::FeatureStateCache value_to_store;
  // Use a loop to preserve logging bits that might be set concurrently.
  do {
    value_to_store = new_value | (expected & (Feature::kCachedLogGeneralMask |
                                              Feature::kCachedLogEarlyMask));
  } while (!cached_value.compare_exchange_weak(expected, value_to_store,
                                               std::memory_order_relaxed,
                                               std::memory_order_relaxed));
}

// Variant 2: Explaining helper rationale using local APIs and self-contained context.
bool IsLink(const FilePath& file_path) {
  // Query the filesystem directly with GetFileAttributes() to avoid stale data.
  return (::GetFileAttributes(file_path.value().c_str()) &
          FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}
```
