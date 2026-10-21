---
name: base/claim-ownership-with-nonzero-size-in-mac-malloc-zones
rubric: correctness
severity: should_fix
---

**Rule.** In macOS `malloc_zone_t::size`, `malloc_usable_size`, or
`GetSizeEstimate` implementations, do not return 0 for an address that falls
within the allocator's managed address space or pool range. When allocation
metadata is inaccessible or superpages are decommitted, return a non-zero
nominal size (such as 1) to claim ownership and prevent the OS zone dispatcher
from incorrectly querying other zones.

**Why.** On Apple platforms, the macOS malloc zone dispatcher interprets a
return value of 0 from `malloc_zone_t::size()` as an indication that the zone
does not own the pointer, prompting it to search other registered zones or crash
during deallocation. Returning a nominal non-zero value (such as 1 byte)
confirms ownership without dereferencing invalid or decommitted metadata.

## Examples

Bad:

```cpp
#if PA_BUILDFLAG(IS_APPLE)
if (!partition_alloc::IsManagedByPartitionAlloc(address_as_uintptr)) {
  return 0;
}
if (!partition_alloc::internal::ReservationOffsetTable::Get(
         address_as_uintptr)
         .IsManagedByNormalBucketsOrDirectMap(address_as_uintptr)) {
  // Metadata is unavailable or superpage was decommitted.
  return 0;
}
#endif
```

Good:

```cpp
#if PA_BUILDFLAG(IS_APPLE)
if (!partition_alloc::IsManagedByPartitionAlloc(address_as_uintptr)) {
  return 0;
}
if (!partition_alloc::internal::ReservationOffsetTable::Get(
         address_as_uintptr)
         .IsManagedByNormalBucketsOrDirectMap(address_as_uintptr)) {
  // Return 1 (not 0) to indicate we own this address within our pool range
  // even if metadata is unavailable. Returning 0 tells macOS we don't own it.
  return 1;
}
#endif
```
