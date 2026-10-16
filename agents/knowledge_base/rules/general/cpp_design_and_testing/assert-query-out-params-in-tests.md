---
name: general/assert-query-out-params-in-tests
rubric: testing
severity: nit
---

**Rule.** In unit tests, when calling query, search, or lookup methods that
populate an output parameter (such as an index, position, count, or result item)
where the expected state is deterministic, assert the expected value of the
output parameter in addition to checking the boolean return status.

**Why.** Merely asserting a boolean success or presence flag leaves output
variables uninspected, failing to verify exact internal state (such as the
expected item position or freelist slot) and masking subtle ordering or
state-tracking bugs.

## Examples

Bad:

```cpp
size_t pos1, pos2;
EXPECT_TRUE(tcache1->IsInFreelist(
    internal::SlotStart::Unchecked(ptr1).Untag(), bucket_index, pos1));
EXPECT_TRUE(tcache2->IsInFreelist(
    internal::SlotStart::Unchecked(ptr2).Untag(), bucket_index, pos2));
```

Good:

```cpp
size_t pos1, pos2;
EXPECT_TRUE(tcache1->IsInFreelist(
    internal::SlotStart::Unchecked(ptr1).Untag(), bucket_index, pos1));
EXPECT_EQ(pos1, 0u);
EXPECT_TRUE(tcache2->IsInFreelist(
    internal::SlotStart::Unchecked(ptr2).Untag(), bucket_index, pos2));
EXPECT_EQ(pos2, 0u);
```
