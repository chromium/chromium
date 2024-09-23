# PartitionAlloc Design

This document describes PartitionAlloc at a high level, with some architectural
details. For implementation details, see the comments in
`partition_alloc_constants.h`.

## Quick Links

* [Glossary](./glossary.md): Definitions of terms commonly used in
  PartitionAlloc. The present document largely avoids defining terms.

* [Build Config](./build_config.md): Pertinent GN args, buildflags, and
  macros.

* [Chrome-External Builds](./external_builds.md): Further considerations
  for standalone PartitionAlloc, plus an embedder's guide for some extra
  GN args.

## Overview

PartitionAlloc is a memory allocator optimized for space efficiency,
allocation latency, and security.

### Performance

PartitionAlloc is designed to be extremely fast in its fast paths. The fast
paths of allocation and deallocation require very few (reasonably predictable)
branches. The number of operations in the fast paths is minimal, leading to the
possibility of inlining.

![The central allocator manages slots and spans. It is locked on a
  per-partition basis. Separately, the thread cache consumes slots
  from the central allocator, allowing it to hand out memory
  quickly to individual threads.](./src/partition_alloc/dot/layers.png)

However, even the fast path isn't the fastest, because it requires taking
a per-partition lock. Although we optimized the lock, there was still room for
improvement; to this end, we introduced the thread cache.
The thread cache has been tailored to satisfy a vast majority of requests by
allocating from and releasing memory to the main allocator in batches,
amortizing lock acquisition and further improving locality while not trapping
excess memory.

### Security

Security is one of the important goals of PartitionAlloc.

PartitionAlloc guarantees that different partitions exist in different regions
of the process's address space. When the caller has freed all objects contained
in a page in a partition, PartitionAlloc returns the physical memory to the
operating system, but continues to reserve the region of address space.
PartitionAlloc will only reuse an address space region for the same partition.

Similarly, one page can contain only objects from the same bucket.
When freed, PartitionAlloc returns the physical memory, but continues to reserve
the region for this very bucket.

The above techniques help avoid type confusion attacks. Note, however, these
apply only to normal buckets and not to direct map, as it'd waste too much
address space.

PartitionAlloc also guarantees that:

* Linear overflows/underflows cannot corrupt into, out of, or between
  partitions. There are guard pages at the beginning and the end of each memory
  region owned by a partition.

* Linear overflows/underflows cannot corrupt the allocation metadata.
  PartitionAlloc records metadata in a dedicated, out-of-line region (not
  adjacent to objects), surrounded by guard pages. (Freelist pointers are an
  exception.)

* Partial pointer overwrite of freelist pointer should fault.

* Direct map allocations have guard pages at the beginning and the end.

### Alignment

PartitionAlloc guarantees that returned pointers are aligned on
`partition_alloc::internal::kAlignment` boundary (typically 16B on
64-bit systems, and 8B on 32-bit).

PartitionAlloc also supports higher levels of alignment, that can be requested
via `PartitionAlloc::AlignedAlloc()` or platform-specific APIs (such as
`posix_memalign()`). The requested
alignment has to be a power of two. PartitionAlloc reserves the right to round
up the requested size to the nearest power of two, greater than or equal to the
requested alignment. This may be wasteful, but allows taking advantage of
natural PartitionAlloc alignment guarantees. Allocations with an alignment
requirement greater than `partition_alloc::internal::kAlignment` are expected
to be very rare.

## Architecture

### Layout in Memory

PartitionAlloc handles normal buckets by reserving (not committing) 2MiB super
pages. Each super page is split into partition pages.
The first and the last partition page are permanently inaccessible and serve
as guard pages, with the exception of one system page in the middle of the first
partition page that holds metadata (32B struct per partition page).

![A super page is shown full of slot spans. The slot spans are logically
  strung together to form buckets. At both extremes of the super page
  are guard pages. PartitionAlloc metadata is hidden inside the
  guard pages at the "front."](./src/partition_alloc/dot/super-page.png)

* The slot span numbers provide a visual hint of their size (in partition
  pages).
* Colors provide a visual hint of the bucket to which the slot span belongs.
    * Although only five colors are shown, in reality, a super page holds
      tens of slot spans, some of which belong to the same bucket.
* The system page that holds metadata tracks each partition page with one 32B
  [`PartitionPageMetadata` struct][PartitionPage], which is either
    * a [`SlotSpanMetadata`][SlotSpanMetadata] ("v"s in the diagram) or
    * a [`SubsequentPageMetadata`][SubsequentPageMetadata] ("+"s in the
      diagram).
* Gray fill denotes guard pages (one partition page each at the head and tail
  of each super page).
* In some configurations, PartitionAlloc stores more metadata than can
  fit in the one system page at the front. These are the bitmaps for
  `MTECheckedPtr<T>`, and they are relegated to the head of
  what would otherwise be usable space for slot spans. One, both, or
  none of these bitmaps may be present, depending on build
  configuration, runtime configuration, and type of allocation.
  See [`SuperPagePayloadBegin()`][payload-start] for details.

As allocation requests arrive, there is eventually a need to allocate a new slot
span.
Address space for such a slot span is carved out from the last super page. If
not enough space, a new super page is allocated. Due to varying sizes of slot
span, this may lead to leaving space unused (we never go back to fill previous
super pages), which is fine because this memory is merely reserved, which is far
less precious than committed memory. Note also that address space reserved for a
slot span is never released, even if the slot span isn't used for a long time.

All slots in a newly allocated slot span are *free*, i.e. available for
allocation.

### Freelist Pointers

All free slots within a slot span are chained into a singly-linked free-list,
by writing the *next* pointer at the beginning of each slot, and the head of the
list is written in the metadata struct.

However, writing a pointer in each free slot of a newly allocated span would
require committing and faulting in physical pages upfront, which would be
unacceptable. Therefore, PartitionAlloc has a concept of *provisioning slots*.
Only provisioned slots are chained into the freelist.
Once provisioned slots in a span are depleted, then another page worth of slots
is provisioned (note, a slot that crosses a page boundary only gets
provisioned with slots of the next page). See
`PartitionBucket::ProvisionMoreSlotsAndAllocOne()` for more details.

Freelist pointers are stored at the beginning of each free slot. As such, they
are the only metadata that is inline, i.e. stored among the
objects. This makes them prone to overruns. On little-endian systems, the
pointers are encoded by reversing byte order, so that partial overruns will very
likely result in destroying the pointer, as opposed to forming a valid pointer
to a nearby location.

Furthermore, a shadow of a freelist pointer is stored next to it, encoded in a
different manner. This helps PartitionAlloc detect corruptions.

### Slot Span States

A slot span can be in any of 4 states:
* *Full*. A full span has no free slots.
* *Empty*. An empty span has no allocated slots, only free slots.
* *Active*. An active span is anything in between the above two.
* *Decommitted*. A decommitted span is a special case of an empty span, where
  all pages are decommitted from memory.

PartitionAlloc prioritizes getting an available slot from an active span, over
an empty one, in hope that the latter can be soon transitioned into a
decommitted state, thus releasing memory. There is no mechanism, however, to
prioritize selection of a slot span based on the number of already allocated
slots.

An empty span becomes decommitted either when there are too many empty spans
(FIFO), or when `PartitionRoot::PurgeMemory()` gets invoked periodically (or in
low memory pressure conditions). An allocation can be satisfied from
a decommitted span if there are no active or empty spans available. The slot
provisioning mechanism kicks back in, committing the pages gradually as needed,
and the span becomes active. (There is currently no other way
to unprovision slots than decommitting the entire span).

As mentioned above, a bucket is a collection of slot spans containing slots of
the same size. In fact, each bucket has 3 linked-lists, chaining active, empty
and decommitted spans (see `PartitionBucket::*_slot_spans_head`).
There is no need for a full span list. The lists are updated lazily. An empty,
decommitted or full span may stay on the active list for some time, until
`PartitionBucket::SetNewActiveSlotSpan()` encounters it.
A decommitted span may stay on the empty list for some time,
until `PartitionBucket::SlowPathAlloc()` encounters it. However,
the inaccuracy can't happen in the other direction, i.e. an active span can only
be on the active list, and an empty span can only be on the active or empty
list.

[PartitionPage]: https://source.chromium.org/search?q=-file:third_party/(angle|dawn)%20class:PartitionPageMetadata%20file:partition_page.h&ss=chromium
[SlotSpanMetadata]: https://source.chromium.org/search?q=-file:third_party/(angle|dawn)%20class:SlotSpanMetadata%20file:partition_page.h&ss=chromium
[SubsequentPageMetadata]: https://source.chromium.org/search?q=-file:third_party/(angle|dawn)%20class:SubsequentPageMetadata%20file:partition_page.h&ss=chromium
[payload-start]: https://source.chromium.org/search?q=-file:third_party%2F(angle%7Cdawn)%20content:SuperPagePayloadBegin%20file:partition_page.h&ss=chromium
