# Chrome-External Builds

Work is ongoing to make PartitionAlloc a standalone library. The
standalone repository for PartitionAlloc is hosted
[here][standalone-PA-repo].

## GN Args

External clients mainly need to set these six GN args:

``` none
# These are blocked on PA-E and `raw_ptr.h` and can never be true until
# we make them part of the standalone PA distribution.
use_partition_alloc_as_malloc_default = false
enable_mte_checked_ptr_support_default = false
enable_backup_ref_ptr_support_default = false
put_ref_count_in_previous_slot_default = false
enable_backup_ref_ptr_slow_checks_default = false
enable_dangling_raw_ptr_checks_default = false
```

PartitionAlloc's build will expect them at
`//build_overrides/partition_alloc.gni`.

In addition, something must provide `build_with_chromium = false` to
the PA build system.

## `use_partition_alloc`

The `use_partition_alloc` GN arg, described in
[`build_config.md`](./build_config.md), provides a GN-level seam that
embedders

1.  can set in their GN args and
2.  should  observe in their GN recipes to conditionally pull in
    PartitionAlloc.

I.E. if you have any reason to disable PartitionAlloc, you should do so
with this GN arg. Avoid pulling in PartitionAlloc headers when the
corresponding buildflag is false.

Setting `use_partition_alloc` false will also implicitly disable other
features, e.g. nixing the compilation of BackupRefPtr as the
implementation of `raw_ptr<T>`.

## Periodic Memory Reduction Routines

PartitionAlloc provides APIs to

* reclaim memory (see [memory\_reclaimer.h](./memory_reclaimer.h)) and

* purge thread caches (see [thread\_cache.h](./thread_cache.h)).

Both of these must be called by the embedder external to PartitionAlloc.
PA provides neither an event loop nor timers of its own, delegating this
to its clients.

## Build Considerations

External clients create constraints on PartitionAlloc's implementation.

### C++17

PartitionAlloc targets C++17. This is aligned with our first external
client, PDFium, and may be further constrained by other clients. These
impositions prevent us from moving in lockstep with Chrome's target
C++ version.

We do not even have guarantees of backported future features, e.g.
C++20's designated initializers. Therefore, these cannot ship with
PartitionAlloc.

### MSVC Support

PDFium supports MSVC. PartitionAlloc will have to match it.

### MSVC Constraint: No Inline Assembly

MSVC's syntax for `asm` blocks differs from the one widely adopted in
parts of Chrome. But more generally,
[MSVC doesn't support inline assembly on ARM and x64 processors][msvc-inline-assembly].
Assembly blocks should be gated behind compiler-specific flags and
replaced with intrinsics in the presence of `COMPILER_MSVC` (absent
`__clang__`).

[standalone-PA-repo]: https://chromium.googlesource.com/chromium/src/base/allocator/partition_allocator.git
[msvc-inline-assembly]: https://docs.microsoft.com/en-us/cpp/assembler/inline/inline-assembler?view=msvc-170
