---
name: base/avoid-redundant-inline-variants-in-partitionalloc
rubric: safety
severity: should_fix
---

**Rule.** In PartitionAlloc (base/allocator/partition_allocator/\*\*), do not
introduce dedicated FooInline (PA_ALWAYS_INLINE) variants alongside PA_NOINLINE
wrappers unless the function is performance-critical and called from multiple
hot paths. Additionally, in allocator shims and inlined dispatchers, avoid
branching on trivial or default arguments to call separate specialized inlined
functions; dispatch through a unified function and handle base cases internally.

**Why.** Forcing inlining via PA_ALWAYS_INLINE across multiple call sites or
branching between specialized inlined routines duplicates large inlined bodies,
significantly increasing instruction cache pressure and binary size.

## Examples

Bad:

```cpp
// Redundant PA_NOINLINE wrapper without active callers:
template <FreeFlags flags = FreeFlags::kNone>
PA_NOINLINE void FreeWithSizeAndAlignment(void* object,
                                          size_t size,
                                          size_t alignment) {
  FreeWithSizeAndAlignmentInline<flags>(object, size, alignment);
}

// Branching in shim dispatch to separate inlined functions:
if (alignment <= partition_alloc::internal::kAlignment) {
  return PartitionRoot::FreeWithSizeInlineInUnknownRoot<flags>(object, size);
}
PartitionRoot::FreeWithSizeAndAlignmentInlineInUnknownRoot<flags>(
    object, size, alignment);
```

Good:

```cpp
// Keep single PA_ALWAYS_INLINE variant when callers do not need wrappers:
template <FreeFlags flags = FreeFlags::kNone>
PA_ALWAYS_INLINE void FreeWithSizeAndAlignment(void* object,
                                               size_t size,
                                               size_t alignment);

// Dispatch through a single inlined function, handling default alignment internally:
PartitionRoot::FreeWithSizeAndAlignmentInlineInUnknownRoot<flags>(
    object, size, alignment);

// Handled internally in size adjustment helper:
if (alignment <= internal::kAlignment) {
  return requested_size;
}
```
