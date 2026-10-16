---
name: general/use-get-cached-for-cached-value-accessors
rubric: readability
severity: nit
---

**Rule.** Use the past-participle prefix `GetCached...` (or `cached_...` /
`getCached...`) rather than `GetCache...` when naming accessors that return a
cached value, setting, or snapshot. Reserve `GetCache...` (or `getCache...`) for
accessors that return a cache storage instance, cache directory/path, cache key,
or cache subsystem.

**Why.** Naming a value getter `GetCache<Property>()` misleadingly implies
returning a cache data structure, cache directory, or cache configuration
object. Using `GetCached<Property>()` clearly conveys that the function
retrieves a cached value rather than returning the cache itself.

## Examples

Bad:

```cpp
// Misleadingly implies returning a cache instance or configuration object.
BASE_EXPORT int GetCacheLowMemoryDeviceThresholdMb();
```

Good:

```cpp
// Clearly conveys returning a value retrieved from cache.
BASE_EXPORT int GetCachedLowMemoryDeviceThresholdMb();
```
