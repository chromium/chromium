# Chrome-External Builds

Work is ongoing to make PartitionAlloc a standalone library. The
standalone repository for PartitionAlloc is hosted
[here][standalone-PA-repo].

## GN Args

External clients should examine the args described in
`build_overrides/partition_alloc.gni` and add them in their own source
tree. PartitionAlloc's build will expect them at
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

* reclaim memory (see `memory_reclaimer.h`) and

* purge thread caches (see `thread_cache.h`).

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
