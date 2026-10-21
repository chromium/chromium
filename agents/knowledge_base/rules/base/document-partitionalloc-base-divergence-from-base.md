---
name: base/document-partitionalloc-base-divergence-from-base
rubric: readability
severity: nit
---

**Rule.** When low-level allocator utilities in partition_alloc_base
deliberately diverge from their //base counterparts (such as omitting
thread_local caching, dynamic allocation, or platform helpers to avoid allocator
reentrancy), add an explanatory code comment. Document why the code diverges
from //base, the trade-off involved (such as syscall overhead vs. reentrancy
safety), and cite the tracking issue (e.g. crbug.com/<id>).

**Why.** partition_alloc_base is intended to be a mostly unmutated subset of
//base. Documenting intentional divergences and their technical trade-offs
prevents future maintainers from inadvertently reintroducing allocator
reentrancy bugs or allocation hazards during freshening merges.

## Examples

Bad:

```cpp
#elif PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
  static InitAtFork init_at_fork;
  return syscall(__NR_gettid);
```

Good:

```cpp
#elif PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
  // Unlike base/threading/platform_thread_posix.cc, we avoid using thread_local
  // for thread_id caching. This prevents reentrancy issues within
  // PartitionAlloc, as reported in crbug.com/476192650. Since
  // partition_alloc::internal::base::PlatformThread::CurrentId is called
  // infrequently, the performance overhead of not using thread_local is
  // acceptable and outweighed by avoiding reentrancy during allocation.
  return syscall(__NR_gettid);
```
