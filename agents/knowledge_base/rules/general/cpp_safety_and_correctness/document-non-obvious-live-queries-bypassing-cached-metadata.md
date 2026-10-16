---
name: general/document-non-obvious-live-queries-bypassing-cached-metadata
rubric: correctness
severity: should_fix
---

**Rule.** When performing live system, kernel, or filesystem queries (such as
querying file attributes or calling `base::IsLink()`) to bypass cached
enumeration metadata and avoid stale data or TOCTOU races, do not introduce
ad-hoc fallback branches that revert to stale or cached metadata upon query
failure. Handle query failures explicitly or rely deterministically on the
authoritative query helper, and add an inline comment explaining why fresh
system state is required.

**Why.** Falling back to stale cached metadata when a live query fails
introduces non-deterministic control flow and defeats TOCTOU protections or
cache-invalidation safeguards. Documenting the live query prevents future
maintainers from mistakenly refactoring it back to cached metadata or adding
unverified fallbacks.

## Examples

Bad:

```cpp
// Stale fallback masks query failure and defeats fresh-data guarantees.
DWORD attributes = ::GetFileAttributes(current.value().c_str());
if (attributes == INVALID_FILE_ATTRIBUTES) {
  attributes = info.find_data().dwFileAttributes;
}
if (recursive && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
  DeleteFileRecursive(current, pattern, true);
}
```

Good:

```cpp
if (recursive) {
  // Use `IsLink` to ensure fresh filesystem data is queried deterministically
  // to avoid TOCTOU reparse-point traversal without falling back to stale cache.
  if (!IsLink(current)) {
    DeleteFileRecursive(current, pattern, true);
  }
}
```
