---
name: base/no-base-dependencies-in-partitionalloc
rubric: design
severity: should_fix
---

**Rule.** Do not add dependencies on Chromium's '//base' or bypass GN dependency
checks with '// nogncheck' inside PartitionAlloc
('base/allocator/partition_allocator/'). Place tests and utilities requiring
'//base' services (such as 'base::RawPtrAsanService' or
'base::debug::AsanService') into '//base' test targets instead.

**Why.** PartitionAlloc is maintained as an independent standalone library
rolled into external embedders (such as V8, Dawn, and PDFium) that do not
include Chromium's //base. Bypassing GN dependency checks with '// nogncheck'
introduces hidden dependencies and breaks standalone embedder builds.

## Examples

Bad:

```cpp
// In base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ref_unittest.cc:
#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#include "base/debug/asan_service.h"
#include "base/memory/raw_ptr_asan_service.h"  // nogncheck
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
```

Good:

```cpp
// In base/memory/raw_ptr_asan_unittest.cc (outside PartitionAlloc):
#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#include "base/debug/asan_service.h"
#include "base/memory/raw_ptr_asan_service.h"
// Run integration tests against PartitionAlloc APIs from //base test suites.
#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
```
