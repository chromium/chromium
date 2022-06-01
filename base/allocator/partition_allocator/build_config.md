# Build Config

PartitionAlloc's behavior and operation can be influenced by many
different settings. Broadly, these are controlled at the top-level by
[GN args][gn-declare-args], which propagate via
[buildflags][buildflag-header] and `#defined` clauses.

*** promo
Most of what you'll want to know exists between

* [`//base/allocator/partition_allocator/BUILD.gn`][pa-build-gn],
* [`allocator.gni`][allocator-gni],
* [`//base/allocator/BUILD.gn`][base-allocator-build-gn], and
* [`//base/BUILD.gn`][base-build-gn].
***

*** aside
While Chromium promotes the `#if BUILDFLAG(FOO)` construct, some of
PartitionAlloc's behavior is governed by compound conditions `#defined`
in [`partition_alloc_config.h`][partition-alloc-config].
***

## Select GN Args

### `use_partition_alloc`

Defines whether PartitionAlloc is at all available.

Setting this `false` will entirely remove PartitionAlloc from the
Chromium build. _You probably do not want this._

*** note
Back when PartitionAlloc was the dedicated allocator in Blink, disabling
it was logically identical to wholly disabling it in Chromium. This GN
arg organically grew in scope with the advent of
PartitionAlloc-Everywhere and must be `true` as a prerequisite for
enabling PA-E.
***

### `use_allocator`

Does nothing special when value is `"none"`. Enables
[PartitionAlloc-Everywhere (PA-E)][pae-public-doc] when value is
`"partition"`.

*** note
* While "everywhere" (in "PartitionAlloc-Everywhere") tautologically
  includes Blink where PartitionAlloc originated, setting
  `use_allocator = "none"` does not disable PA usage in Blink.
* `use_allocator = "partition"` internally sets
  `use_partition_alloc_as_malloc = true`, which must not be confused
  with `use_partition_alloc` (see above).
***

### `use_backup_ref_ptr`

Specifies `BackupRefPtr` as the implementation for `base::raw_ptr<T>`
when `true`. See the [MiraclePtr documentation][miracleptr-doc].

*** aside
BRP requires support from PartitionAlloc, so `use_backup_ref_ptr` also
compiles the relevant code into PA. However, this arg does _not_ govern
whether or not BRP is actually enabled at runtime - that functionality
is controlled by a Finch flag.
***

## Note: Component Builds

When working on PartitionAlloc, know that `is_debug` defaults to
implying `is_component_build`, which interferes with the allocator
shim. A typical set of GN args should include

```none
is_debug = true
is_component_build = false
```

Conversely, build configurations that have `is_component_build = true`
without explicitly specifying PA-specific args will not build with PA-E
enabled.

## Notable Macros

There is an ongoing effort
[to break out PartitionAlloc into a standalone library][pa-ee-crbug].
Once PartitionAlloc stands alone from the larger Chrome build apparatus,
the code loses access to some macros. This is not an immediate concern,
but the team needs to decide either

* how to propagate these macros in place, or
* how to remove them, replacing them with PA-specific build config.

A non-exhaustive list of work items:

* `OFFICIAL_BUILD` - influences crash macros and
  `PA_THREAD_CACHE_ALLOC_STATS`. These are conceptually distinct enough
  to be worth separating into dedicated build controls.
* `IS_PARTITION_ALLOC_IMPL` - must be defined when PartitionAlloc is
  built as a shared library. This is required to export symbols.
* `COMPONENT_BUILD` - component builds (as per
  `//docs/component_build.md`) must `#define COMPONENT_BUILD`.
  Additionally, to build Win32, invoker must `#define WIN32`.
* `MEMORY_TOOL_REPLACES_ALLOCATOR`
* `*_SANITIZER` - mainly influences unit tests.

TODO(crbug.com/1151236): don't `PA_COMPONENT_EXPORT()` functions defined
under `partition_alloc_base/`.

*** note
Over time, the above list should evolve into a list of macros / GN args
that influence PartitionAlloc's behavior.
***

[gn-declare-args]: https://gn.googlesource.com/gn/+/refs/heads/main/docs/reference.md#func_declare_args
[buildflag-header]: https://source.chromium.org/chromium/chromium/src/+/main:build/buildflag_header.gni
[pa-build-gn]: https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/BUILD.gn
[allocator-gni]: https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/allocator.gni
[base-allocator-build-gn]: https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/BUILD.gn
[base-build-gn]: https://source.chromium.org/chromium/chromium/src/+/main:base/BUILD.gn
[partition-alloc-config]: https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/partition_alloc_config.h
[pae-public-doc]: https://docs.google.com/document/d/1R1H9z5IVUAnXJgDjnts3nTJVcRbufWWT9ByXLgecSUM/preview
[miracleptr-doc]: https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg/preview
[pa-ee-crbug]: https://crbug.com/1151236
