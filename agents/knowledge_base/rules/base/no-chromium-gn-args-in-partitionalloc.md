---
name: base/no-chromium-gn-args-in-partitionalloc
rubric: design
severity: blocking
---

**Rule.** Do not reference Chromium-specific GN variables or build arguments
inside PartitionAlloc (`base/allocator/partition_allocator/`). Define
Chromium-specific feature flags and conditional logic in outer Chromium layers
(e.g., `base/allocator/`) using Chromium's `BUILDFLAG(...)` instead of adding
flags to `PA_BUILDFLAG(...)` headers.

**Why.** PartitionAlloc is maintained as a standalone library rolled into
external embedders (such as V8, Dawn, and PDFium) that do not define Chromium GN
arguments. Referencing Chromium-specific variables in PartitionAlloc's GN
configurations breaks standalone embedder builds.

## Examples

Bad:

```cpp
pa_buildflag_header("buildflags") {
  header = "buildflags.h"
  flags = [
    "IS_ANDROID=$is_android",
    "IS_DESKTOP_ANDROID=$is_desktop_android",
  ]
}
```

Good:

```cpp
// In base/allocator/partition_alloc_features.cc (outside PartitionAlloc):
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
    BackupRefPtrEnabledProcesses::kNonRenderer,
#else
    BackupRefPtrEnabledProcesses::kAllProcesses,
#endif
```
