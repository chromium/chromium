# Glossary

This page describes some core terminology used in PartitionAlloc.
A weak attempt is made to present terms "in conceptual order" s.t.
each term depends mainly upon previously defined ones.

### Partition

A heap that is separated and protected both from other
partitions and from non-PartitionAlloc memory. Each partition holds
multiple buckets.

*** promo
**NOTE**: In code (and comments), "partition," "root," and even
"allocator" are all conceptually the same thing.
***

## Pages

### System Page

A memory page defined by the CPU/OS. Commonly
referred to as a "virtual page" in other contexts. This is typically
4KiB, but it can be larger. PartitionAlloc supports up to 64KiB,
though this constant isn't always known at compile time (depending
on the OS).

### Partition Page

The most common granularity used by
PartitionAlloc. Consists of exactly 4 system pages.

### Super Page

A 2MiB region, aligned on a 2MiB boundary. Not to
be confused with OS-level terms like "large page" or "huge page",
which are also commonly 2MiB. These have to be fully committed /
uncommitted in memory, whereas super pages can be partially committed
with system page granularity.

### Extent

An extent is a run of consecutive super pages (belonging
to a single partition). Extents are to super pages what slot spans are
to slots (see below).

## Slots and Spans

### Slot

An indivisible allocation unit. Slot sizes are tied to
buckets. For example, each allocation that falls into the bucket
(224,&nbsp;256] would be satisfied with a slot of size 256. This
applies only to normal buckets, not to direct map.

### Slot Span

A run of same-sized slots that are contiguous in
memory. Slot span size is a multiple of partition page size, but it
isn't always a multiple of slot size, although we try hard for this
to be the case.

### Small Bucket

Allocations up to 4 partition pages. In these
cases, slot spans are always between 1 and 4 partition pages in
size. For each slot span size, the slot span is chosen to minimize
number of pages used while keeping the rounding waste under a
reasonable limit.

*   For example, for a slot size 96, 64B waste is deemed acceptable
    when using a single partition page, but for slot size
    384, the potential waste of 256B wouldn't be, so 3 partition pages
    are used to achieve 0B waste.
*   PartitionAlloc may avoid waste by lowering the number of committed
    system pages compared to the number of reserved pages. For
    example, for the slot size of 896B we'd use a slot span of 2
    partition pages of 16KiB, i.e. 8 system pages of 4KiB, but commit
    only up to 7, thus resulting in perfect packing.

### Single-Slot Span

Allocations above 4 partition pages (but
&le;`kMaxBucketed`). This is because each slot span is guaranteed to
hold exactly one slot.

*** promo
Fun fact: there are sizes &le;4 partition pages that result in a
slot span having exactly 1 slot, but nonetheless they're still
classified as small buckets. The reason is that single-slot spans
are often handled by a different code path, and that distinction
is made purely based on slot size, for simplicity and efficiency.
***

## Buckets

### Bucket

A collection of regions in a partition that contains
similar-sized objects. For example, one bucket may hold objects of
size (224,&nbsp;256], another (256,&nbsp;320], etc. Bucket size
brackets are geometrically spaced,
[going up to `kMaxBucketed`][max-bucket-comment].

*** promo
Plainly put, all slots (ergo the resulting spans) of a given size
class are logically chained into one bucket.
***

![A bucket, spanning multiple super pages, collects spans whose
  slots are of a particular size class.](./src/partition_alloc/dot/bucket.png)

### Normal Bucket

Any bucket whose size ceiling does not exceed
`kMaxBucketed`. This is the common case in PartitionAlloc, and
the "normal" modifier is often dropped in casual reference.

### Direct Map (Bucket)

Any allocation whose size exceeds `kMaxBucketed`.

## Other Terms

### Object

A chunk of memory returned to the allocating invoker
of the size requested. It doesn't have to span the entire slot,
nor does it have to begin at the slot start. This term is commonly
used as a parameter name in PartitionAlloc code, as opposed to
`slot_start`.

### Thread Cache

A [thread-local structure][pa-thread-cache] that
holds some not-too-large memory chunks, ready to be allocated. This
speeds up in-thread allocation by reducing a lock hold to a
thread-local storage lookup, improving cache locality.

### Pool

A large (and contiguous on 64-bit) virtual address region, housing
super pages, etc. from which PartitionAlloc services allocations. The
primary purpose of the pools is to provide a fast answer to the
question, "Did PartitionAlloc allocate the memory for this pointer
from this pool?" with a single bit-masking operation.

*   The regular pool is a general purpose pool that contains allocations that
    aren't protected by BackupRefPtr.
*   The BRP pool contains all allocations protected by BackupRefPtr.
*   [64-bit only] The configurable pool is named generically, because its
    primary user (the [V8 Sandbox][v8-sandbox]) can configure it at runtime,
    providing a pre-existing mapping. Its allocations aren't protected by
    BackupRefPtr.
*   [64-bit only] The thread isolated pool is returning memory protected with
    per-thread permissions. At the moment, this is implemented for pkeys on x64.
    It's primary user is [V8 CFI][v8-cfi].

![The singular AddressPoolManager mediates access to the separate pools
  for each PartitionRoot.](./src/partition_alloc/dot/address-space.png)

*** promo
Pools are downgraded into a logical concept in 32-bit environments,
tracking a non-contiguous set of allocations using a bitmap.
***

### Payload

The usable area of a super page in which slot spans
reside. While generally this means "everything between the first
and last guard partition pages in a super page," the presence of
other metadata can bump the starting offset
forward. While this term is entrenched in the code, the team
considers it suboptimal and is actively looking for a replacement.

### Allocation Fast Path

A path taken during an allocation that is
considered fast.  Usually means that an allocation request can be
immediately satisfied by grabbing a slot from the freelist of the
first active slot span in the bucket.

### Allocation Slow Path

Anything which is not fast (see above).

Can involve

*   finding another active slot span in the list,
*   provisioning more slots in a slot span,
*   bringing back a free (or decommitted) slot span,
*   allocating a new slot span, or even
*   allocating a new super page.

*** aside
By "slow" we may mean something as simple as extra logic (`if`
statements etc.), or something as costly as system calls.
***

## Legacy Terms

These terms are (mostly) deprecated and should not be used. They are
surfaced here to provide a ready reference for readers coming from
older design documents or documentation.

### GigaCage

A memory region several gigabytes wide, reserved by
PartitionAlloc upon initialization, from which nearly all allocations
are taken. _Pools_ have overtaken GigaCage in conceptual importance,
and so and so there is less need today to refer to "GigaCage" or the
"cage." This is especially true given the V8 Sandbox and the
configurable pool (see above).

## PartitionAlloc-Everywhere

Originally, PartitionAlloc was used only in Blink (Chromium's rendering engine).
It was invoked explicitly, by calling PartitionAlloc APIs directly.

PartitionAlloc-Everywhere is the name of the project that brought PartitionAlloc
to the entire-ish codebase (exclusions apply). This was done by intercepting
`malloc()`, `free()`, `realloc()`, aforementioned `posix_memalign()`, etc. and
routing them into PartitionAlloc. The shim located in
`base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h` is
responsible for intercepting. For more details, see
[base/allocator/README.md](../../../base/allocator/README.md).

A special, catch-it-all *Malloc* partition has been created for the intercepted
`malloc()` et al. This is to isolate from already existing Blink partitions.
The only exception from that is Blink's *FastMalloc* partition, which was also
catch-it-all in nature, so it's perfectly fine to merge these together, to
minimize fragmentation.

As of 2022, PartitionAlloc-Everywhere is supported on

*   Windows 32- and 64-bit
*   Linux
*   Android 32- and 64-bit
*   macOS
*   Fuchsia

[max-bucket-comment]: https://source.chromium.org/search?q=-file:third_party%2F(angle%7Cdawn)%20file:partition_alloc_constants.h%20symbol:kMaxBucketed$&ss=chromium
[pa-thread-cache]: https://source.chromium.org/search?q=-file:third_party%2F(angle%7Cdawn)%20file:partition_alloc/thread_cache.h&ss=chromium
[v8-sandbox]: https://docs.google.com/document/d/1FM4fQmIhEqPG8uGp5o9A-mnPB5BOeScZYpkHjo0KKA8/preview#
[v8-cfi]: https://docs.google.com/document/d/1O2jwK4dxI3nRcOJuPYkonhTkNQfbmwdvxQMyXgeaRHo/preview#
