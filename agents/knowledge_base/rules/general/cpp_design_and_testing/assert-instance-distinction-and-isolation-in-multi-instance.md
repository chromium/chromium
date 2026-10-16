---
name: general/assert-instance-distinction-and-isolation-in-multi-instance
rubric: testing
severity: should_fix
---

**Rule.** When testing support for multiple concurrent instances, caches,
partitions, or resource pools, assert both that distinct instances are
instantiated (e.g. distinct pointers or identifiers) and that operations route
resources into their corresponding instance's state. Avoid tests that only
invoke operations across multiple contexts without verifying state isolation and
instance distinction.

**Why.** Multi-instance or multiplexed tests that only allocate and free without
assertions can falsely pass even if the contexts inadvertently share a single
fallback instance or route resources into the wrong partition. Verifying
distinct instance identities and checking state residency ensures isolation is
genuinely exercised.

## Examples

Bad:

```cpp
// In a test verifying support for multiple independent caches or partitions:
auto* ptr1 = root1->Alloc(kTestAllocSize);
auto* ptr2 = root2->Alloc(kTestAllocSize);

root1->Free(ptr1);
root2->Free(ptr2);
// Flawed: passes even if root1 and root2 inadvertently share a single cache instance.
```

Good:

```cpp
auto* ptr1 = root1->Alloc(kTestAllocSize);
auto* ptr2 = root2->Alloc(kTestAllocSize);
root1->Free(ptr1);
root2->Free(ptr2);

ThreadCache* tcache1 = ThreadCache::Get(1);
ThreadCache* tcache2 = ThreadCache::Get(2);
// Assert distinct instances were created:
EXPECT_NE(tcache1, tcache2);

// Assert resources routed to each respective instance's state:
size_t bucket_index = SizeToIndex(kTestAllocSize);
size_t pos1, pos2;
EXPECT_TRUE(tcache1->IsInFreelist(ptr1, bucket_index, pos1));
EXPECT_TRUE(tcache2->IsInFreelist(ptr2, bucket_index, pos2));
```
