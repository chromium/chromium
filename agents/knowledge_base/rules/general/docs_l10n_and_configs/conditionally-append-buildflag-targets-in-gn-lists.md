---
name: general/conditionally-append-buildflag-targets-in-gn-lists
rubric: correctness
severity: nit
---

**Rule.** In GN target aggregation lists, sources, or dependency lists,
conditionally append targets that depend on a feature buildflag using if
(buildflag) { list += [ ... ] } rather than listing them unconditionally in the
default list.

**Why.** If a target is guarded by a feature buildflag or assertion in its
defining BUILD.gn (such as assert(enable_feature)), listing it unconditionally
causes GN evaluation or build dependency errors in configurations where the
feature flag is disabled.

## Examples

Bad:

```cpp
browser_exposed_mojom_targets = [
  "//components/history/core/browser/mojom:mojom",
  "//components/surface_embed/common:mojom",
]
```

Good:

```cpp
import("//components/surface_embed/buildflags/buildflags.gni")

browser_exposed_mojom_targets = [
  "//components/history/core/browser/mojom:mojom",
]

if (enable_surface_embed) {
  browser_exposed_mojom_targets += [ "//components/surface_embed/common:mojom" ]
}
```
