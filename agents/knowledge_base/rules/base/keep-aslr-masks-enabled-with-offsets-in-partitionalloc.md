---
name: base/keep-aslr-masks-enabled-with-offsets-in-partitionalloc
rubric: safety
severity: should_fix
---

**Rule.** In PartitionAlloc's address space randomization (`ASLRMask()` and
`ASLROffset()`), do not set `ASLRMask` to 0 even in constrained address spaces.
Instead, clamp `AslrMask` to the platform's supported bit width and define
`ASLROffset` with `AslrAddress` matching the platform's minimum base mapping
address.

**Why.** Setting `ASLRMask()` to 0 causes `GetRandomPageBase()` to return 0,
passing a null address hint to `mmap()`. On platforms with large pool alignment
requirements (e.g. 4 GiB or 8 GiB), unguided allocations often return unaligned
addresses, forcing repeated allocation attempts or expensive over-allocation and
virtual address trimming.

## Examples

Bad:

```cpp
#elif PA_BUILDFLAG(IS_IOS)
// Disabling ASLR in constrained environments passes 0 hint to mmap.
PA_ALWAYS_INLINE constexpr uintptr_t ASLRMask() {
  return AslrMask(0);
}
PA_ALWAYS_INLINE constexpr uintptr_t ASLROffset() {
  return AslrAddress(0);
}
```

Good:

```cpp
#elif PA_BUILDFLAG(IS_IOS)
// Clamp mask to 35 bits (32 GiB) to stay below hardware limits and provide aligned hints.
PA_ALWAYS_INLINE constexpr uintptr_t ASLRMask() {
  return AslrMask(35);
}
PA_ALWAYS_INLINE constexpr uintptr_t ASLROffset() {
  return AslrAddress(0x300000000ULL);
}
```
