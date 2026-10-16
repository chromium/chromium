---
name: general/clean-up-platform-packaging-deps-on-target-removal
rubric: correctness
severity: should_fix
---

**Rule.** When migrating a standalone asset to a resource bundle or deleting a
GN target, clean up all platform-specific packaging dependencies (such as
Mac/iOS bundle_deps/bundle_data and non-Apple data_deps) across all build files.
Do not leave orphaned packaging dependencies in platform-guarded targets.

**Why.** Migrating an asset or deleting a target without removing its
corresponding platform-specific packaging references leaves orphaned target
dependencies. This can cause build failures, broken incremental builds, or
redundant asset bundling.

## Examples

Bad:

```cpp
# In chrome/BUILD.gn, retaining bundle_deps after migrating asset to resources.pak:
if (is_mac) {
  bundle_deps += [
    "//third_party/search_engines_data:prepopulated_engines_bundle_data",
  ]
}
```

Good:

```cpp
# Removed bundle_deps entry when the target was deleted and migrated to resources.pak:
if (is_mac) {
  # Cleanly removed obsolete prepopulated_engines_bundle_data dependency
}
```
