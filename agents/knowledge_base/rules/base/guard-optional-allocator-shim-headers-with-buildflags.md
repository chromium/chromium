---
name: base/guard-optional-allocator-shim-headers-with-buildflags
rubric: correctness
severity: should_fix
---

**Rule.** In PartitionAlloc's allocator shim layer, guard optional symbol
interception headers (such as allocator_shim_alloc_token_symbols\*.h) and symbol
override definitions with their corresponding PA_BUILDFLAG(...) condition (e.g.,
#if PA_BUILDFLAG(SHIM_SUPPORTS_ALLOC_TOKEN)). Do not include optional symbol
headers or export shim symbols unconditionally.

**Why.** Unconditionally including symbol-interception headers or exporting
allocator shim symbols produces unnecessary exports and causes build or link
failures in configurations where the feature is unsupported or disabled.

## Examples

Bad:

```cpp
#include "partition_alloc/shim/allocator_shim_alloc_token_symbols.h"
```

Good:

```cpp
#if PA_BUILDFLAG(SHIM_SUPPORTS_ALLOC_TOKEN)
#include "partition_alloc/shim/allocator_shim_alloc_token_symbols.h"
#endif
```
