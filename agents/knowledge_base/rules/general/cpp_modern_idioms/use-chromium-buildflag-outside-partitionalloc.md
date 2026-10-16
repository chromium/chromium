---
name: general/use-chromium-buildflag-outside-partitionalloc
rubric: readability
severity: should_fix
---

**Rule.** Do not use PartitionAlloc's internal `PA_BUILDFLAG(...)` macro for
platform checks (such as `IS_ANDROID`, `IS_WIN`, `IS_APPLE`) or Chromium feature
flags in code outside standalone `base/allocator/partition_allocator/`. Use
Chromium's standard `BUILDFLAG(...)` from `build/build_config.h` or the
appropriate Chromium `buildflags.h` header instead.

**Why.** `PA_BUILDFLAG(...)` is reserved for PartitionAlloc's internal
standalone code to avoid symbol and namespace conflicts in external embedders.
Chromium code should consistently use standard `BUILDFLAG(...)` macros from
`build/build_config.h`.

## Examples

Bad:

```cpp
#if PA_BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
    BackupRefPtrEnabledProcesses::kNonRenderer,
#else
    BackupRefPtrEnabledProcesses::kAllProcesses,
#endif
```

Good:

```cpp
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
    BackupRefPtrEnabledProcesses::kNonRenderer,
#else
    BackupRefPtrEnabledProcesses::kAllProcesses,
#endif
```
