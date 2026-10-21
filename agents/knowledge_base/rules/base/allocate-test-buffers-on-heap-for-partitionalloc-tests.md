---
name: base/allocate-test-buffers-on-heap-for-partitionalloc-tests
rubric: correctness
severity: nit
---

**Rule.** When writing unit tests that verify PartitionAlloc metadata, slot
boundaries, quarantine behavior, or checked span bounds enforcement, allocate
test buffers on the heap (such as via std::make_unique) rather than on the
stack.

**Why.** PartitionAlloc only manages heap-allocated memory pools. Stack storage
bypasses PartitionAlloc metadata tracking and slot layouts, causing
IsManagedByPartitionAlloc() queries to return false and allocator verification
tests to fail.

## Examples

Bad:

```cpp
// Stack allocation is not managed by PartitionAlloc:
std::array<char, 32> buffer = {};
EXPECT_TRUE(partition_alloc::IsManagedByPartitionAlloc(
    partition_alloc::UntagPtr(buffer.data())));  // Fails!
```

Good:

```cpp
// Heap allocation ensures the buffer is managed by PartitionAlloc:
auto buffer = std::make_unique<std::array<char, 32>>();
EXPECT_TRUE(partition_alloc::IsManagedByPartitionAlloc(
    partition_alloc::UntagPtr(buffer->data())));  // Passes
```
