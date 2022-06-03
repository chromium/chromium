# PartitionAlloc Design

This document describes PartitionAlloc at a high level, with some architectural
details. For implementation details, see the comments in
`partition_alloc_constants.h`.

## Overview

PartitionAlloc is a memory allocator optimized for space efficiency,
allocation latency, and security.

### Core terms

A *partition* is a heap that is separated and protected from any other
partitions, as well as from non-PartitionAlloc memory. The most typical use of
partitions is to isolate certain object types. However, one can also isolate
objects of certain sizes, or objects of a certain lifetime (as the caller
prefers). Callers can create as many partitions as they need. The direct
memory cost of partitions is minimal, but the implicit cost resulting from
fragmentation is not to be underestimated.

Each partition holds multiple buckets. A *bucket* is a series of regions in a
partition that contains similar-sized objects, e.g. one bucket holds sizes
(240,&nbsp;256], another (256,&nbsp;288], and so on. Bucket sizes are
geometrically-spaced, and go all the way up to `kMaxBucketed=960KiB`
(so called *normal buckets*). There are 8 buckets between each power of two.
Note that buckets that aren't a multiple of `base::kAlignment` can't be used.

Larger allocations (&gt;`kMaxBucketed`) are realized by direct memory mapping
(*direct map*).

### Performance

PartitionAlloc is designed to be extremely fast in its fast paths. The fast
paths of allocation and deallocation require very few (reasonably predictable)
branches. The number of operations in the fast paths is minimal, leading to the
possibility of inlining.

However, even the fast path isn't the fastest, because it requires taking
a per-partition lock. Although we optimized the lock, there was still room for
improvement. Therefore we introduced the *thread cache*, which holds a small
amount of not-too-large memory chunks, ready to be allocated. Because these
chunks are stored per-thread, they can be allocated without a lock, only
requiring a faster thread-local storage (TLS) lookup, improving cache locality
in the process.
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
`base::kAlignment` boundary (typically 16B on 64-bit systems, and 8B on 32-bit).

PartitionAlloc also supports higher levels of alignment, that can be requested
via `PartitionAlloc::AlignedAllocFlags()` or platform-specific APIs (such as
`posix_memalign()`). The requested
alignment has to be a power of two. PartitionAlloc reserves the right to round
up the requested size to the nearest power of two, greater than or equal to the
requested alignment. This may be wasteful, but allows taking advantage of
natural PartitionAlloc alignment guarantees. Allocations with an alignment
requirement greater than `base::kAlignment` are expected to be very rare.

## PartitionAlloc-Everywhere

Originally, PartitionAlloc was used only in Blink (Chromium’s rendering engine).
It was invoked explicitly, by calling PartitionAlloc APIs directly.

PartitionAlloc-Everywhere is the name of the project that brought PartitionAlloc
to the entire-ish codebase (exclusions apply). This was done by intercepting
`malloc()`, `free()`, `realloc()`, aforementioned `posix_memalign()`, etc. and
routing them into PartitionAlloc. The shim located in
`base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h` is
responsible for intercepting. For more details, see
[base/allocator/README.md](../../../base/allocator/README.md).

A special, catch-it-all *Malloc* partition has been created for the intercepted
`malloc()` et al. This is to isolate from already existing Blink partitions.
The only exception from that is Blink's *FastMalloc* partition, which was also
catch-it-all in nature, so it's perfectly fine to merge these together, to
minimize fragmentation.

PartitionAlloc-Everywhere was launched in M89 for Windows 64-bit and Android.
Windows 32-bit and Linux followed it shortly after, in M90.

## Architecture

### Many Different Flavors of Pages

In PartitionAlloc, by *system page* we mean a memory page as defined by CPU/OS
(often referred to as "virtual page" out there). It is most commonly 4KiB in
size, but depending on CPU it can be larger (PartitionAlloc supports up to
64KiB).

The reason why we use the term "system page" is to disambiguate from
*partition page*, which is the most common granularity used by PartitionAlloc.
Each partition page consists of exactly 4 system pages.

A *super page* is a 2MiB region, aligned on a 2MiB boundary.
Don't confuse it with CPU/OS terms like "large page" or "huge page", which are
also commonly 2MiB in size. These have to be fully committed/uncommitted in
memory, whereas super pages can be partially committed, with system page
granularity.

### Slots and Spans

A *slot* is an indivisible allocation unit. Slot sizes are tied to buckets.
For example each allocation that falls into the bucket (240;&nbsp;256] would
be satisfied with a slot of size 256. This applies only to normal buckets, not
to direct map.

A *slot span* is just a grouping of slots of the same size next to each other
in memory. Slot span size is a multiple of a partition page.

A bucket is a collection of slot spans containing slots of the same size,
organized as linked-lists.

Allocations up to 4 partition pages are referred to as *small buckets*.
In these cases, slot spans are always between 1 and 4 partition pages in size.
The size is chosen based on the slot size, such that the rounding waste is
minimized. For example, if the slot size was 96B and slot span was 1 partition
page of 16KiB, 64B would be wasted at the end, but nothing is wasted if 3
partition pages totalling 48KiB are used. Furthermore, PartitionAlloc may avoid
waste by lowering the number of committed system pages compared to the number of
reserved pages. For example, for the slot size of 80B we'd use a slot span of 4
partition pages of 16KiB, i.e. 16 system pages of 4KiB, but commit only up to
15, thus resulting in perfect packing.

Allocations above 4 partition pages (but &le;`kMaxBucketed`) are referred to as
*single slot spans*. That's because each slot span is guaranteed to hold exactly
one slot. Fun fact: there are sizes &le;4 partition pages that result in a slot
span having exactly 1 slot, but nonetheless they're still classified as small
buckets. The reason is that single slot spans are often handled by a different
code path, and that distinction is made purely based on slot size, for
simplicity and efficiency.

### Layout in Memory

PartitionAlloc handles normal buckets by reserving (not committing) 2MiB super
pages. Each super page is split into partition pages.
The first and the last partition page are permanently inaccessible and serve
as guard pages, with the exception of one system page in the middle of the first
partition page that holds metadata (32B struct per partition page).

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
until `PartitionBucket<thread_safe>::SlowPathAlloc()` encounters it. However,
the inaccuracy can't happen in the other direction, i.e. an active span can only
be on the active list, and an empty span can only be on the active or empty
list.
